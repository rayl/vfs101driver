/* vfs101 fingerprint driver 
 * 
 * Copyright (c) 2010 Damir Syabitov <dsyabitov@gmail.com>
 * Copyright (c) 2010 Ray Lehtiniemi <rayl@mail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <libusb-1.0/libusb.h>


/* The device seems to send back 16 frames of 292 bytes at a time */
const unsigned int PKTSIZE = 292;
const unsigned int N_PKTS = 16;

#define nitems(x) (sizeof(x)/sizeof(x[0]))


/******************************************************************************************************
 * Context structure for this driver.
 */
struct vfs_dev {
	/* context object for libusb library */
	struct libusb_context *ctx;

	/* libusb device handle for fingerprint reader */
	struct libusb_device_handle *devh;

	/* sequence number for current send/recv transaction pair */
	unsigned short seq;

	/* The last response from the device, valid immediately after a recv() */
	unsigned char buf[0x40];
	int len;

	/* buffer to hold raw image data packets */
	unsigned char ibuf[1024*1024];
	int ilen;
	int inum;
};


/******************************************************************************************************
 * Functions to deal with byte swapping.
 */
static inline unsigned char b0 (int n)
{
	return n & 0xff;
}

static inline unsigned char b1 (int n)
{
	return (n>>8) & 0xff;
}

static inline unsigned char b2 (int n)
{
	return (n>>16) & 0xff;
}

static inline unsigned char b3 (int n)
{
	return (n>>24) & 0xff;
}

static inline unsigned short xx (int h, int l)
{
	return (h<<8)|l;
}


/******************************************************************************************************
 * Debug printing routines
 */

static int dump_packet (unsigned char *data, int length, unsigned char *prefix)
{
	int i = 0;
	fprintf(stdout, "%s ", prefix);
	for (i; i < length; i++)
		fprintf(stdout, "%02X ", data[i]);
	fprintf(stdout, "\n");
	return length;
}

static int dump_frame_1 (unsigned char *d, int n)
{
	int i;

	fprintf(stdout, "\n  ---------------------------- Packet %05d -----------------------------\n", n);
	d += dump_packet(d,  4, "  HDR 1: ");
	fprintf(stdout, "\n");

	d += dump_packet(d,  2, "  IMG A:       ");
	d += dump_packet(d, 16, "               ");
	for (i=1; i<12; i++)
		d += dump_packet(d, 16, "               ");
	d += dump_packet(d, 8, "               ");
	fprintf(stdout, "\n");

	d += dump_packet(d,  2, "  HDR 2: ");
	fprintf(stdout, "\n");

	d += dump_packet(d, 16, "  IMG B:       ");
	d += dump_packet(d, 16, "               ");
	d += dump_packet(d,  6, "               ");
	fprintf(stdout, "\n");

	d += dump_packet(d, 16, "  IMG C:       ");
	d += dump_packet(d, 10, "               ");
	fprintf(stdout, "\n");

	d += dump_packet(d,  4, "  HDR 3: ");
	fprintf(stdout, "\n");

	d += dump_packet(d, 16, "  IMG D:       ");
	fprintf(stdout, "\n");
}

static int dump_frame (unsigned char *data, int length, int n)
{
	int skip = 0;

	// skip bytes as required until a frame header is found
	while ((length > 1) && ((data[0] != 0x01) || ((data[1] != 0xfe) && (data[1] != 0x01)))) {
		length--;
		data++;
		skip++;
	}

	// warn if we skipped any data
	if (skip > 0)
		fprintf(stdout, "*** Frame misalignment, skipped %d bytes!!\n", skip);

	// dump short frames as raw data
	if (length < PKTSIZE) {
		fprintf(stdout, "*** Short frame, dumping as %d raw bytes!!\n", length);
		dump_packet(data, length, "");
		return skip + length;
	}

	// do the actual printing
	dump_frame_1(data, n);

	// return number of bytes consumed
	return skip + PKTSIZE;
}

static void dump_image (unsigned char *data, int length)
{
	int f = 0;

	fprintf(stdout, "  %d packets in %d bytes%s\n", length/PKTSIZE, length, (length%PKTSIZE) ? " (incomplete packet(s)?)" : "");

	while (length > 0) {
		int n = dump_frame(data, length, f++);
		data += n;
		length -= n;
	}

	fprintf(stdout, "\n");
}

static void dump_pnm_2 (FILE *pnm, int offset, int len)
{
	while (len--) {
		switch (offset++) {
		case 0:
		case 206:
		case 246:
		case 272:
			fprintf(pnm, " 255");
			break;
		default:
			fprintf(pnm, "   0");
			break;
		}
	}
	fprintf(pnm, "\n");
}

static void dump_pnm_1 (struct vfs_dev *dev, unsigned char dir, int offset, int len)
{
	unsigned char *data = dev->ibuf;
	int length = dev->ilen;
	char name[40];
	FILE *pnm;
	int i;

	sprintf(name, "img/%c/out-%03d-%02x.%s", dir, dev->inum, dev->inum, dev->ilen ? "pnm" : "pnmx");
	dev->inum++;

	pnm = fopen(name, "w");

	fprintf(pnm, "P2\n%d %d\n256\n", len, (length/PKTSIZE)+10);

	
	for (i=0; i<5; i++)
		dump_pnm_2(pnm, offset, len);

	while (length > 0) {
		unsigned char *x = data;
		int i;

		data += offset;
		for (i=0; i<len; i++)
			fprintf(pnm, " % 3d", *x++);
		fprintf(pnm, "\n");

		length -= PKTSIZE;
		data += PKTSIZE;
	}

	for (i=0; i<5; i++)
		dump_pnm_2(pnm, offset, len);


	fclose(pnm);
}

static void dump_pnm (struct vfs_dev *dev)
{
	dump_pnm_1(dev, 'X',   0, 292);
	dump_pnm_1(dev, 'A',   0, 206);
	dump_pnm_1(dev, 'B', 206,  40);
	dump_pnm_1(dev, 'C', 246,  26);
	dump_pnm_1(dev, 'D', 272,  20);
}


/******************************************************************************************************
 * Low level send/receive functions
 */

#define EP_IN(n)     (n | LIBUSB_ENDPOINT_IN)
#define EP_OUT(n)    (n | LIBUSB_ENDPOINT_OUT)

#define BULK_TIMEOUT 100

/* The first two bytes of data will be overwritten with seq */
static int send(struct vfs_dev *dev, unsigned char *data, size_t len)
{
	int transferred;
	int r;

	data[0] = b0(dev->seq);
	data[1] = b1(dev->seq);

	dump_packet(data, len, "  --->");
	r = libusb_bulk_transfer(dev->devh, EP_OUT(1), data, len, &transferred, BULK_TIMEOUT);

	if (r < 0) {
		fprintf(stderr, "bulk write error %d", r);
		return r;

	} else if (transferred < len) {
		fprintf(stderr, "unexpected short write %d/%zd", transferred, len);
		return -EIO;

	} else {
		return 0;
	}
}

static int recv(struct vfs_dev *dev)
{
	int transferred;
	int r;

	r = libusb_bulk_transfer(dev->devh, EP_IN(1), dev->buf, 0x40, &dev->len, BULK_TIMEOUT);

	if (r < 0 && r != -7) {
		fprintf(stderr, "bulk read error %d", r);
		return r;
	}

	dump_packet(dev->buf, dev->len, "  <---");
	if ((b0(dev->seq) != dev->buf[0]) || (b1(dev->seq) != dev->buf[1])) {
		fprintf(stderr, "*********** Seqnum mismatch, got %04x, expected %04x\n", xx(dev->buf[1],dev->buf[0]), dev->seq);
	}
	fprintf(stdout, "\n");

	dev->seq++;

	return 0;
}

static int load (struct vfs_dev *dev, unsigned char *buf, int *len)
{
	int n;

	*len = 0;

	do {
		int r = libusb_bulk_transfer(dev->devh, EP_IN(2), buf, N_PKTS*PKTSIZE, &n, BULK_TIMEOUT);

		buf += n;
		*len += n;

		if (r < 0 && r != -7) {
			//fp_err("bulk read error %d", r);
			return r;
		}

	} while (n == N_PKTS*PKTSIZE);

	return 0;
}

static int swap (struct vfs_dev *dev, unsigned char *data, size_t len)
{
	int r;
	if ((r = send(dev, data, len)) < 0)
		return r;
	usleep(2000);
	if ((r = recv(dev)) < 0)
		return r;
	return 0;
}


/******************************************************************************************************
 * Protocol-level API routines

     00 00 01 00    - Reset
     00 00 02 00    - GetVersion
     00 00 03 00    - GetPrint
     00 00 04 00    - GetParam
     00 00 05 00    - SetParam
     00 00 06 00    - GetConfig
     00 00 07 00      DownloadPatch
     00 00 08 00      GetRateData
     00 00 09 00      IspRequest
     00 00 0A 00      ProgramFlash
     00 00 0B 00      EraseFlash
     00 00 0C 00      LedStates
     00 00 0D 00      LedEvent
     00 00 0E 00    - AbortPrint
     00 00 0F 00      Spare2
     00 00 10 00      Spare3
     00 00 11 00      Spare4
     00 00 12 00    - Peek
     00 00 13 00    - Poke
     00 00 14 00      SensorSpiTrans
     00 00 15 00      SensorGPIO
     00 00 16 00    - GetFingerState 
*/

#define _() fprintf(stdout, "%s\n", __FUNCTION__)

/* Reset (00 00 01 00)
 *
 *  Cause the device to reenumerate on the USB bus.
 */
static int Reset (struct vfs_dev *dev)
{
	unsigned char q1[0x06] = { 0x00, 0x00, 0x00, 0x00, 0x01, 0x00 };
	_();
	return swap (dev, q1, 0x06);
}

/* GetVersion (00 00 01 00)
 *
 *  Retrieve version string from the device.
 */
static int GetVersion (struct vfs_dev *dev)
{
	unsigned char q1[0x07] = { 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00 };
	_();
	return swap (dev, q1, 0x07);
}

/* GetPrint (00 00 03 00)
 *
 *  Retrieve fingerprint image information.
 */
static unsigned char type_0[6] = "\x00\x01\x00\x00\x00\x01";
static unsigned char type_1[6] = "\x01\x00\x00\x00\x01\x01";
static int GetPrint (struct vfs_dev *dev, int count, unsigned char args[6])
{
	unsigned char q1[0x0e] = { 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
	int i;
	q1[6] = b0(count);
	q1[7] = b1(count);
	for (i=0; i<6; i++) q1[8+i] = args[i];
	_();
	return swap (dev, q1, 0x0e);
}

/* GetParam (00 00 04 00)
 *
 *  Retrieve a parameter value from the device.
 */
static int GetParam (struct vfs_dev *dev, unsigned short param)
{
	unsigned char q1[0x08] = { 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00 };
	q1[6] = b0(param);
	q1[7] = b1(param);
	_();
	return swap (dev, q1, 0x08);
}

/* SetParam (00 00 05 00)
 *
 *  Set a parameter value on the device.
 */
static int SetParam (struct vfs_dev *dev, unsigned short param, unsigned short value)
{
	unsigned char q1[0x0a] = { 0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00 };
	q1[6] = b0(param);
	q1[7] = b1(param);
	q1[8] = b0(value);
	q1[9] = b1(value);
	_();
	return swap (dev, q1, 0x0a);
}

struct set_param {
	unsigned short param;
	unsigned short value;
};

static int SetParamList (struct vfs_dev *dev, struct set_param *params, int nparams)
{
	int r, i;
	for (i = 0; i < nparams; i++)
		if ((r = SetParam(dev, params[i].param, params[i].value)) < 0)
			return r;
	return 0;
}

/* GetConfig (00 00 06 00)
 *
 *  Retrieve config info from the device.
 */
static int GetConfig (struct vfs_dev *dev)
{
	unsigned char q1[0x06] = { 0x00, 0x00, 0x00, 0x00, 0x06, 0x00 };
	_();
	return swap (dev, q1, 0x06);
}

/* AbortPrint (00 00 0e 00)
 *
 *  Abort the current scan operation.
 */
static int AbortPrint (struct vfs_dev *dev)
{
	unsigned char q1[0x06] = { 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00 };
	_();
	return swap (dev, q1, 0x06);
}

/* Peek (00 00 12 00)
 *
 *  Retrieve a value from the device.
 */
static int Peek (struct vfs_dev *dev, unsigned int addr, unsigned int size)
{
	unsigned char q1[0x0b] = { 0x00, 0x00, 0x00, 0x00, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
	q1[6]  = b0(addr);
	q1[7]  = b1(addr);
	q1[8]  = b2(addr);
	q1[9]  = b3(addr);
	q1[10] = b0(size);
	_();
	return swap (dev, q1, 0x0b);
}

/* Poke (00 00 13 00)
 *
 *  Store a value to the device.
 */
static int Poke (struct vfs_dev *dev, unsigned int addr, unsigned int value, unsigned int size)
{
	unsigned char q1[0x0f] = { 0x00, 0x00, 0x00, 0x00, 0x13, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
	q1[6]  = b0(addr);
	q1[7]  = b1(addr);
	q1[8]  = b2(addr);
	q1[9]  = b3(addr);
	q1[10] = b0(value);
	q1[11] = b1(value);
	q1[12] = b2(value);
	q1[13] = b3(value);
	q1[14] = b0(size);
	_();
	return swap (dev, q1, 0x0f);
}

/* GetFingerState (00 00 16 00)
 *
 *  Poll device for current finger state.
 */
static int GetFingerState (struct vfs_dev *dev)
{
	unsigned char q1[0x06] = { 0x00, 0x00, 0x00, 0x00, 0x16, 0x00 };
	int r;
	_();
	if ((r = swap (dev, q1, 0x06)) < 0)
		return r;
	return dev->buf[0x0a];
}

static int LoadImage (struct vfs_dev *dev)
{
	int r;
	_();
	r = load(dev, dev->ibuf, &dev->ilen);
	if (r == 0) {
		dump_image(dev->ibuf, dev->ilen);
		dump_pnm(dev);
	}
	return r;
}

#undef _


/******************************************************************************************************
 * Memory map definitions
 */

const unsigned int VFS_IMAGE_ABCD = 0x00FF5032;
/*-----------------------------------------------------------------------------------------
   VFS_IMG_ABCD - 8 bits

   This register clearly demonstrates that type 0 image slices have four subimages,
   named A, B, C and D.

    Header 1: 000-003 (4 bytes)

    Image A:  004-205 (202 bytes)
      - if bit 1 is set,      image A is white, finish (128 cases)
      - if bit 3 is set,      image A is black, finish ( 64 cases)
      - if bit 4 XOR bit 7,   image A is black, finish ( 32 cases)
      - else                  image A is gray          ( 32 cases)

    Header 2: 206-207 (2 bytes)
      - seems either white or black

    Image B:  208-245 (38 bytes)
      - same rules as Image A

    Image C:  246-271 (26 bytes)
      - more complex pattern

    Header 3: 272-275 (4 bytes)
      - seems fairly constant

    Image D:  276-291 (16 bytes)
      - same rules as Image C

 */


const unsigned int VFS_CONTRAST = 0x00FF5038;
/*-----------------------------------------------------------------------------------------
   VFS_CONTRAST - 7 bits

   This register controls the contrast. When the lower 7 bits are zero, the image
   has low contrast. Constrast increases as value approached 0x7f, then cycles over
   from 128 to 255.
 */

const unsigned int VFS_TEST = 0x00FF503E;


/******************************************************************************************************
 * Cycle routines
 *
 * Each routine is a particular testcase, selectable from the command line.
 */

/* A shorthand for checking return codes */
static int r;
#define _(x) if ((r = x) != 0) return r

static int check_finger (struct vfs_dev *dev, int n)
{
	while (n--) {
		if ((r = GetFingerState(dev)) < 0)
			return r;
		usleep(50000);
	}
	return 0;
}

static int img_0 (struct vfs_dev *dev, int nframes)
{
	_(  GetPrint  (dev, nframes, type_0));
	_(  LoadImage (dev));
	return 0;
}

static int img_1 (struct vfs_dev *dev, int nframes)
{
	_(  GetPrint  (dev, nframes, type_1));
	_(  LoadImage (dev));
	return 0;
}

static int img_abort (struct vfs_dev *dev)
{
	_(  AbortPrint (dev));
	_(  LoadImage  (dev));
	return 0;
}

static int check_six (struct vfs_dev *dev)
{
	_(  Peek (dev, 0x00001FE8, 0x04));
	_(  Peek (dev, 0x00001FEC, 0x04));
	_(  Peek (dev, 0x00001FF0, 0x04));
	_(  Peek (dev, 0x00001FF4, 0x04));
	_(  Peek (dev, 0x00001FF8, 0x04));
	_(  Peek (dev, 0x00001FFC, 0x04));
	return 0;
}

static int check_1 (struct vfs_dev *dev)
{
	_(  img_abort (dev));
	_(  GetParam  (dev, 0x11));
	_(  SetParam  (dev, 0x0062, 0x0032));
	_(  img_1     (dev, 5000));
	return 0;
}

static int get_a (struct vfs_dev *dev)
{
	_(  GetParam   (dev, 0x2e));
	_(  GetVersion (dev));
	_(  GetParam   (dev, 0x28));
	_(  GetParam   (dev, 0x14));
	return 0;
}

static int get_b (struct vfs_dev *dev)
{
	_(  GetParam  (dev, 0x11));
	_(  GetParam  (dev, 0x54));
	_(  GetParam  (dev, 0x01));
	_(  GetParam  (dev, 0x14));
	_(  img_abort (dev));
	return 0;
}

static int try (struct vfs_dev *dev, unsigned int val)
{
	_(  Poke (dev, VFS_CONTRAST, val, 0x01));
	_(  img_0 (dev, 10));
	return 0;
}

static int pat_1 (struct vfs_dev *dev, unsigned int v1, unsigned int v2)
{
	_(  Poke (dev, 0x000005F6, v1, 0x01));
	_(  Peek (dev, 0x00FF503E, 0x01));
	_(  Poke (dev, 0x00FF503E, v2, 0x01));
	_(  Peek (dev, 0x00FF9802, 0x01));
	_(  Peek (dev, 0x00FF9800, 0x01));
	_(  Peek (dev, 0x00FF9806, 0x01));
	return 0;
}

static int validity_cycle4 (struct vfs_dev *dev)
{
	usleep (100000);
	_(  check_six (dev));
	_(  get_a (dev));
	return 0;
}

static int validity_cycle3 (struct vfs_dev *dev)
{
	_(  check_six  (dev));
	_(  get_a      (dev));
	_(  get_b      (dev));
	_(  img_0      (dev, 1));
	_(  img_abort  (dev));
	struct set_param p1[] = {
		{ 0x0004, 0x0000 },
		{ 0x0005, 0x0000 },
		{ 0x0006, 0x0000 },
		{ 0x0007, 0x0000 },
		{ 0x000A, 0x0002 },
		{ 0x000B, 0x010B },
		{ 0x000C, 0x010C },
		{ 0x000D, 0x010D },
		{ 0x000E, 0x0001 },
		{ 0x0010, 0x0000 },
		{ 0x0012, 0x0040 },
		{ 0x0015, 0x04B0 },
		{ 0x0016, 0x001B },
		{ 0x0017, 0x0001 },
		{ 0x0018, 0x0003 },
		{ 0x0019, 0x0001 },
		{ 0x001A, 0x0000 },
		{ 0x001B, 0x0004 },
		{ 0x001D, 0x010C },
		{ 0x001E, 0x010D },
		{ 0x001F, 0x0000 },
		{ 0x0020, 0x0000 },
		{ 0x0021, 0x0001 },
		{ 0x0022, 0x0001 },
		{ 0x0023, 0x0001 },
		{ 0x0024, 0x0000 },
		{ 0x002C, 0x0046 },
		{ 0x002D, 0x002A },
		{ 0x002F, 0x010E },
		{ 0x0030, 0x010F },
		{ 0x0032, 0x0004 },
		{ 0x0033, 0x0003 },
		{ 0x0034, 0x0005 },
		{ 0x0035, 0x0010 },
		{ 0x0036, 0x000A },
		{ 0x0037, 0x0018 },
		{ 0x0038, 0x001E },
		{ 0x0039, 0x00B2 },
		{ 0x003A, 0x0080 },
		{ 0x003B, 0x0000 },
		{ 0x003E, 0x01F4 },
		{ 0x003F, 0x001A },
		{ 0x0040, 0x00F8 },
		{ 0x0042, 0x0001 },
		{ 0x0043, 0x0004 },
		{ 0x0044, 0x0010 },
		{ 0x0045, 0x0005 },
		{ 0x0046, 0x00F5 },
		{ 0x0047, 0x000C },
		{ 0x0048, 0x0000 },
		{ 0x0049, 0x0000 },
		{ 0x0053, 0x0000 },
		{ 0x0056, 0x00B4 },
		{ 0x0057, 0x0096 },
		{ 0x0058, 0x008C },
		{ 0x0059, 0x0064 },
		{ 0x005B, 0x0001 },
		{ 0x005C, 0x0001 },
		{ 0x005D, 0x0020 },
		{ 0x005E, 0x0064 },
		{ 0x005F, 0x00C8 },
		{ 0x0060, 0x00C8 },
		{ 0x0062, 0x0000 },
		{ 0x0064, 0x011A },
		{ 0x0069, 0x0014 },
	};
	_(  SetParamList  (dev, p1, nitems (p1)));
	_(  GetParam      (dev, 0x2a));
	_(  GetParam      (dev, 0x3c));
	_(  GetParam      (dev, 0x4a));
	_(  GetParam      (dev, 0x41));
	_(  GetVersion    (dev));
	_(  SetParam      (dev, 0x0063, 0x0001));
	unsigned char data89[] = "\x59\x00\x00\x00\x14\x00\x05\x00\xAB\x00\x00\x00\x00";
	_(  swap          (dev, data89, (int) sizeof (data89) - 1)); 
	_(  SetParam      (dev, 0x0064, 0x0118));
	_(  GetConfig     (dev));
	_(  SetParam      (dev, 0x0046, 0x00F5));
	_(  SetParam      (dev, 0x0055, 0x0008));
	_(  SetParam      (dev, 0x006D, 0x0032));
	_(  SetParam      (dev, 0x006E, 0x0003));
	_(  GetParam      (dev, 0x52));
	_(  SetParam      (dev, 0x0052, 0x0320));
	_(  img_0         (dev, 1));
	_(  SetParam      (dev, 0x0052, 0x1EB4));
	_(  Peek          (dev, 0x00FF502C, 0x02));
	_(  Peek          (dev, 0x00FF502E, 0x02));
	_(  pat_1         (dev, 0x00000001, 0x00000000));
	_(  Poke          (dev, 0x00FF9806, 0x00000000, 0x01));
	_(  img_0         (dev, 100));
	_(  pat_1         (dev, 0x00000000, 0x00000010));
	_(  img_0         (dev, 100));
	_(  Peek          (dev, VFS_CONTRAST, 0x01));
	_(  Peek          (dev, 0x00FF500E, 0x02));
	_(  Peek          (dev, VFS_IMAGE_ABCD, 0x01));
	_(  Poke          (dev, VFS_IMAGE_ABCD, 0x00000012, 0x01));
	_(  Poke          (dev, 0x00FF500E, 0x00004000, 0x02));
	_(  Poke          (dev, VFS_CONTRAST, 0x0000000F, 0x01));
	_(  SetParam      (dev, 0x0062, 0x0000));
	_(  SetParam      (dev, 0x0077, 0x0000));
	_(  SetParam      (dev, 0x0076, 0x0000));
	_(  SetParam      (dev, 0x0078, 0x0000));
	_(  img_0         (dev, 2));
	_(  pat_1         (dev, 0x00000001, 0x00000000));
	_(  try           (dev, 0x0000000E));
	_(  try           (dev, 0x0000000D));
	_(  try           (dev, 0x0000000C));
	_(  try           (dev, 0x0000000B));
	_(  try           (dev, 0x0000000A));
	_(  try           (dev, 0x00000009));
	_(  try           (dev, 0x00000008));
	_(  try           (dev, 0x00000007));
	_(  pat_1         (dev, 0x00000000, 0x00000010));
	_(  Poke          (dev, 0x00FF9806, 0x00000000, 0x01));
	_(  img_0         (dev, 10));
	_(  SetParam      (dev, 0x0077, 0x0007));
	_(  SetParam      (dev, 0x0076, 0x0012));
	_(  SetParam      (dev, 0x0078, 0x21A0));
	_(  Poke          (dev, VFS_CONTRAST, 0x00000014, 0x01));
	_(  Poke          (dev, 0x00FF500E, 0x000021B4, 0x02));
	_(  Poke          (dev, VFS_IMAGE_ABCD, 0x00000031, 0x01));
	_(  SetParam      (dev, 0x0062, 0x0032));
	_(  img_abort     (dev));
	_(  SetParam      (dev, 0x0062, 0x0032));
	_(  GetParam      (dev, 0x14));
	_(  GetParam      (dev, 0x11));
	_(  SetParam      (dev, 0x0062, 0x0032));
	_(  img_0         (dev, 20));
	_(  GetParam      (dev, 0x14));
	_(  GetParam      (dev, 0x14));
	_(  check_1       (dev));
	_(  check_finger  (dev, 50));
	return 0;
}

static int validity_cycle2 (struct vfs_dev *dev)
{
	_(  check_six    (dev));
	_(  check_finger (dev, 1));
	return 0;
}

static int validity_cycle1 (struct vfs_dev *dev)
{
	_(  check_six (dev));
	_(  get_a     (dev));
	_(  get_b     (dev));
	_(  img_0     (dev, 1));
	_(  img_abort (dev));
	_(  SetParam  (dev, 0x0004, 0x0000));
	_(  LoadImage (dev));
	struct set_param p1[] = {
		{ 0x0005, 0x0000 },
		{ 0x0006, 0x0000 },
		{ 0x0007, 0x0000 },
		{ 0x000A, 0x0002 },
		{ 0x000B, 0x010B },
		{ 0x000C, 0x010C },
		{ 0x000D, 0x010D },
		{ 0x000E, 0x0001 },
		{ 0x0010, 0x0000 },
		{ 0x0012, 0x0040 },
		{ 0x0015, 0x04B0 },
		{ 0x0016, 0x001B },
		{ 0x0017, 0x0001 },
		{ 0x0018, 0x0003 },
	};
	_(  SetParamList (dev, p1, nitems(p1)));
	_(  GetParam     (dev, 0x14));
	_(  GetParam     (dev, 0x14));
	_(  check_1      (dev));
	_(  check_finger (dev, 80));
	return 0;
}

static int validity_cycle0 (struct vfs_dev *dev)
{
	_(  check_six    (dev));
	_(  SetParam     (dev, 0x0062, 0x0032));
//	_(  img_0        (dev, 20));
	_(  check_finger (dev, 10));
	return 0; 
}
	
static int validity_cycle (struct vfs_dev *dev)
{
	_(  Peek    (dev, 0x00001FE8, 0x04));
	_(  Peek    (dev, 0x00001FEC, 0x04));
	_(  img_0   (dev, 1));
	_(  check_1 (dev));
	return 0;	
}

/* Exercise the gain register */
static int test (struct vfs_dev *dev)
{
	int i;
	for (i=0; i<0x100; i++) {
		_(  Poke (dev, VFS_TEST, i, 0x01));
		_(  img_0 (dev, 100));
	}
	return 0;	
}

#undef _


/******************************************************************************************************
 * Main launcher
 *
 * The main routine can launch any of the cycle routines defined above. Simple add an entry
 * to func() with a token for the command line argument and a cycle function to invoke when
 * that argument is seen.
 */

typedef int (*cycle_func)(struct vfs_dev *);

static cycle_func func (const char *id)
{
#define _(x,f) if (strcmp(id, #x) == 0) return f
	if (id != NULL) {
		_(0, validity_cycle0);
		_(1, validity_cycle1);
		_(2, validity_cycle2);
		_(3, validity_cycle3);
		_(4, validity_cycle4);
		_(test, test);
	}
	return validity_cycle;
#undef _
}


/** Main function */
int main (int argc, char **argv)
{
	struct vfs_dev _dev, *dev = &_dev;
	struct sigaction sigact;
	int r = 1;

	dev->seq = 0;

	r = libusb_init(&dev->ctx);
	if (r < 0) {
		fprintf(stderr, "failed to initialise libusb\n");
		exit(1);
	}
	fprintf(stdout, "Libusb inititalized!\n");


	fprintf(stdout, "Searching for device...\n");
	dev->devh = libusb_open_device_with_vid_pid(NULL, 0x138a, 0x0001);
	if (dev->devh == NULL) {
		fprintf(stderr, "Can't find validity device!\n");
		goto out;
	}
	fprintf(stdout, "Device found!\n");

	fprintf(stdout, "Checking active kernel driver...\n");

	int i = 0;
	for (i; i < 1000000; i++){
		r = libusb_kernel_driver_active(dev->devh, i);
		if ( r == 1 ){
			fprintf(stdout, "Detaching kernel driver... \n");
			r = libusb_detach_kernel_driver(dev->devh, 4);
			if (r < 0)
				fprintf(stderr, "Error detaching kernel driver!\n");
			else
				fprintf(stdout, "Kernel driver detached!\n");
		}
	}


	fprintf(stdout, "Claiming interface...\n");
	r = libusb_claim_interface(dev->devh, 0);
	if (r < 0) {
		fprintf(stderr, "usb_claim_interface error %d\n", r);
		goto out;
	}
	fprintf(stdout, "claimed interface\n");

	fprintf(stdout, "Resetting device...\n");
	r = libusb_reset_device(dev->devh);
	if (r != 0)
		fprintf(stdout, "Error resetting device");

	fprintf(stdout, "Configuring device...\n");
	r = libusb_control_transfer(dev->devh, LIBUSB_REQUEST_TYPE_STANDARD, LIBUSB_REQUEST_SET_FEATURE, 1, 1, NULL, 0, BULK_TIMEOUT); 
        if (r < 0) {
		fprintf(stderr, "device configuring error %d\n", r);
		goto out_release;
	}
	fprintf(stdout, "Device configured!\n");
     
	fprintf(stdout, "Entering main cycle...\n");
	r = func(argv[1])(dev);
	if (r != 0) {
		fprintf(stderr, "got error in main cycle %d\n", r);
		goto out_release;
	}
	fprintf(stdout, "Main cycle complete success!\n");

out_release:
	libusb_release_interface(dev->devh, 0);
	libusb_reset_device(dev->devh); 
	libusb_close(dev->devh);
out:
	libusb_exit(dev->ctx);
	return r;
}
