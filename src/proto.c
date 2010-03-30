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
struct result_table;

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

	/* current UsbSnoop results to check against */
	struct result_table *results;

	/* should we mask personal information? */
	int anonymous;
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
	fprintf(stdout, "  {\n");
	d += dump_packet(d,  2, "  Line type       ");
	d += dump_packet(d,  2, "  Sequence        ");
	d += dump_packet(d,  2, "  ???             ");
	fprintf(stdout, "\n");

	d += dump_packet(d, 16, "  Fingerprint A   ");
	for (i=1; i<12; i++)
		d += dump_packet(d, 16, "                  ");
	d += dump_packet(d,  8, "                  ");
	fprintf(stdout, "\n");

	d += dump_packet(d,  2, "  ???             ");
	fprintf(stdout, "\n");

	d += dump_packet(d, 16, "  IMG B           ");
	d += dump_packet(d, 16, "                  ");
	d += dump_packet(d,  6, "                  ");
	d += dump_packet(d, 16, "  IMG C           ");
	d += dump_packet(d, 10, "                  ");
	fprintf(stdout, "\n");

	d += dump_packet(d,  4, "  Constant        ");
	d += dump_packet(d,  2, "  Sequence'       ");
	fprintf(stdout, "\n");

	d += dump_packet(d,  1, "  S_curr_state    ");
	d += dump_packet(d,  1, "  S_next_state    ");
	d += dump_packet(d,  2, "  S_count         ");
	d += dump_packet(d,  2, "  S_level         ");
	d += dump_packet(d,  8, "  ???             ");
	fprintf(stdout, "  }\n");
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

static void dump_image (struct vfs_dev *dev)
{
	int f = 0;
	unsigned char *data = dev->ibuf;
	int length = dev->ilen;

	if (dev->ilen < 101*PKTSIZE) return;
	fprintf(stdout, "  %d packets in %d bytes%s\n", length/PKTSIZE, length, (length%PKTSIZE) ? " (incomplete packet(s)?)" : "");
	fprintf(stdout, "  {\n");

	if (!dev->anonymous) {
		while (length > 0) {
			int n = dump_frame(data, length, f++);
			data += n;
			length -= n;
		}
	}

	fprintf(stdout, "  }\n");
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

	sprintf(name, "img/%c/out-%03d-%02x.%s", dir, dev->inum, dev->inum, dev->ilen ? "pnm" : "pnm");

	pnm = fopen(name, "w");

	if (pnm == NULL) {
		printf("Can't create image file: %s\n", name);
		return;
	}

	fprintf(pnm, "P2\n%d %d\n256\n", len, (length/PKTSIZE)+10);

	
	for (i=0; i<5; i++)
		dump_pnm_2(pnm, offset, len);

	while (length > 0) {
		unsigned char *x = data + offset;
		int i;

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
	if (dev->anonymous) return;
	if (dev->ilen < 101*PKTSIZE) return;
	dump_pnm_1(dev, 'X',   0, 292);
	//dump_pnm_1(dev, 'A',   0, 206);
	//dump_pnm_1(dev, 'B', 206,  40);
	//dump_pnm_1(dev, 'C', 246,  26);
	//dump_pnm_1(dev, 'D', 272,  20);
	dev->inum++;
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
     00 00 14 00    - SensorSpiTrans
     00 00 15 00      SensorGPIO
     00 00 16 00    - GetFingerState 
*/

static int _cmd_no = -1;
#define _() fprintf(stdout, "\n> %s (%d)\n", __FUNCTION__,_cmd_no); _cmd_no=-1

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
static unsigned char type_2[6] = "\x00\x00\x00\x00\x00\x01";
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

/* SensorSpiTrans (00 00 14 00)
 *
 *  ???
 */
static int SensorSpiTrans (struct vfs_dev *dev, unsigned char p1, unsigned char p2, unsigned char p3, unsigned char p4, unsigned char p5, unsigned char p6, unsigned char p7)
{
	unsigned char q1[0x0d] = { 0x00, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
	q1[6]  = p1;
	q1[7]  = p2;
	q1[8]  = p3;
	q1[9]  = p4;
	q1[10] = p5;
	q1[11] = p6;
	q1[12] = p7;
	_();
	return swap (dev, q1, 0x0d);
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
		dump_image(dev);
		dump_pnm(dev);
	}
	return r;
}

#undef _


/******************************************************************************************************
 * Parameter definitions
 */

// 0x2a 0x3c 0x41
static int parm_read[] = 
{
	0x01, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x10,
	0x11, 0x14, 0x15, 0x16, 0x17, 0x19, 0x1a, 0x1b,
	0x1c, 0x1d, 0x1e, 0x28, 0x2c, 0x2e, 0x2f, 0x30,
	0x31, 0x32, 0x33, 0x42, 0x44, 0x45, 0x46, 0x47,
	0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
	0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
	0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
	0x68, 0x69, 0x6a, 0x6d, 0x6e, 0x6f, 0x70, 0x71,
	0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
};

// 0x04 0x05 0x06 0x07 0x12 0x18 0x1f 0x20
// 0x21 0x22 0x23 0x24 0x2d 0x34 0x35 0x36
// 0x37 0x38 0x39 0x3a 0x3b 0x3e 0x3f 0x40
// 0x43
static int parm_write[] = 
{
	0x01, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x10, 0x11,
	0x15, 0x16, 0x17, 0x19, 0x1a, 0x1b, 0x1d, 0x1e,
	0x2c, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x42, 0x44,
	0x45, 0x46, 0x47, 0x48, 0x49, 0x4b, 0x4c, 0x4d,
	0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55,
	0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d,
	0x5e, 0x5f, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65,
	0x66, 0x67, 0x68, 0x69, 0x6a, 0x6d, 0x6e, 0x6f,
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
	0x78,
};

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


const unsigned int VFS_DARKEN = 0x00FF500E;

const unsigned int VFS_DARKEN_CD_1 = 0x00FF502C; // values 0-6 produce no image
const unsigned int VFS_DARKEN_CD_2 = 0x00FF502E; // values 0-6 produce no image

const unsigned int VFS_CONTRAST = 0x00FF5038;
/*-----------------------------------------------------------------------------------------
   VFS_CONTRAST - 7 bits

   This register controls the contrast. When the lower 7 bits are zero, the image
   has low contrast. Constrast increases as value approached 0x7f, then cycles over
   from 128 to 255.
 */


const unsigned int VFS_GRATING = 0x00FF503E;
/*-----------------------------------------------------------------------------------------
   VFS_GRATING - n bits

   This register seems to produce a "grating" effect with bit 1 set.  also, the image
   seems to have a slightly elevated contrast if bits 1, 2, and 3 are zero.
 */

const unsigned int VFS_KILL_4 = 0x00FF9802;
/*-----------------------------------------------------------------------------------------
   VFS_KILL_4 - n bits

   This register kills the device when writing 0x04. Not sure if higher numbers work...
 */

const unsigned int VFS_NO_EFFECT_1 = 0x00FF9806;

const unsigned int VFS_TEST = 0x00FF9800;


/******************************************************************************************************
 * Result checking framework.
 */

/* Result from a single API command when executed on Windows */
struct result {
	int len;
	unsigned char *data;
};

/* All results from a single UsbSnoop.log file */
struct result_table {
	int n;
	struct result r[];
};

/* Look up a result in the result table by URB number */
struct result * res_get (struct result_table *r, int n)
{
	if (n < 0) return NULL;
	if (r == NULL) return NULL;
	if (n > r->n) return NULL;
	if (r->r[n].data == NULL) return NULL;
	return &r->r[n];
}

static void res_check (struct vfs_dev *dev, int n)
{
	struct result *r = res_get(dev->results,n);

	if (r == NULL) {
		printf("  !!!! no result to check against! !!!!\n");

	} else if ((r->len != dev->len - 4) || (memcmp(r->data, dev->buf+4, dev->len-4) != 0)) {
		dump_packet(r->data, r->len, "  XXXX            ");
	}
}


/******************************************************************************************************
 * Cycle routines
 *
 * Each routine is a particular testcase, selectable from the command line.
 */

/* A shorthand for checking return codes */
static int r;
#define _(x) if ((r = x) != 0) return r
#define __(n, x) _cmd_no=n; if ((r = x) != 0) return r; res_check(dev,n)

/* Reset the scanner device */
static int reset (struct vfs_dev *dev)
{
	_(  Reset (dev));
	return 0;	
}

/* Exercise the gain register */
static int test (struct vfs_dev *dev)
{
	int i;
	for (i=0; i<0x100; i++) {
		_(  Poke (dev, VFS_TEST, i, 0x01));
		_(  GetPrint  (dev, 100, type_0));
		_(  LoadImage (dev));
	}
	return 0;	
}

/* Wait for a finger touch */
static int wait_for_touch (struct vfs_dev *dev)
{
	while (GetFingerState(dev) != 2)
		usleep(50000);
	return 0;
}

/* mess with the width of secondary image */
static int parm_c = 0x010c;

/* The frequency of info lines */
static int info_line_rate = 0x32;

/* The best contrast value tried so far */
static int best_contrast = 0x00;

/* Image line exposure level */
static int exposure = 0x21bc;

/* Try a contrast register setting */
static int try_contrast (struct vfs_dev *dev, int value)
{
	_(  Poke (dev, 0x00ff5038, value, 0x01));
	_(  GetPrint (dev, 0x000a, type_0));
	_(  LoadImage (dev));
	// evaluate the result...
	best_contrast = value;
	return 0;
}

/* Find the best contrast setting */
static int scan_contrast (struct vfs_dev *dev)
{
	best_contrast = 0x00;
	_(  try_contrast (dev, 0x0e));
	_(  try_contrast (dev, 0x0d));
	_(  try_contrast (dev, 0x0c));
	_(  try_contrast (dev, 0x0b));
	_(  try_contrast (dev, 0x0a));
	_(  try_contrast (dev, 0x09));
	return 0;
}

/* first working version */
#include "state0.h"
#include "state1.h"
#include "state2.h"
static int woot (struct vfs_dev *dev)
{
	S0_unchecked(dev);
	dev->results = &S1_results;
	S1_checked(dev);
	do {
		wait_for_touch(dev);
		dev->results = &S2_results;
		S2_checked(dev);
	} while (0);
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
#define _(x) if (strcmp(id, #x) == 0) return x
	if (id != NULL) {
		_(reset);
		_(test);
		_(woot);
	}
	return woot;
#undef _
}


/** Main function */
int main (int argc, char **argv)
{
	struct vfs_dev _dev, *dev = &_dev;
	struct sigaction sigact;
	int r = 1;

	dev->seq = 0;
	dev->anonymous = 1;

	if ((argc > 2) && (strcmp(argv[2], "personal") == 0))
		dev->anonymous = 0;

	r = libusb_init(&dev->ctx);
	if (r < 0) {
		fprintf(stderr, "failed to initialise libusb\n");
		exit(1);
	}

	dev->devh = libusb_open_device_with_vid_pid(NULL, 0x138a, 0x0001);
	if (dev->devh == NULL) {
		fprintf(stderr, "Can't find validity device!\n");
		goto out;
	}

	int i = 0;
	for (i; i < 1000000; i++){
		r = libusb_kernel_driver_active(dev->devh, i);
		if ( r == 1 ){
			r = libusb_detach_kernel_driver(dev->devh, 4);
			if (r < 0)
				fprintf(stderr, "Error detaching kernel driver!\n");
		}
	}

	r = libusb_claim_interface(dev->devh, 0);
	if (r < 0) {
		fprintf(stderr, "usb_claim_interface error %d\n", r);
		goto out;
	}

	r = libusb_reset_device(dev->devh);
	if (r != 0) {
		fprintf(stderr, "Error resetting device");
		goto out_release;
	}

	r = libusb_control_transfer(dev->devh, LIBUSB_REQUEST_TYPE_STANDARD, LIBUSB_REQUEST_SET_FEATURE, 1, 1, NULL, 0, BULK_TIMEOUT); 
        if (r < 0) {
		fprintf(stderr, "device configuring error %d\n", r);
		goto out_release;
	}
     
	r = func(argv[1])(dev);
	if (r != 0) {
		fprintf(stderr, "got error in main cycle %d\n", r);
		goto out_release;
	}

out_release:
	libusb_release_interface(dev->devh, 0);
	libusb_reset_device(dev->devh); 
	libusb_close(dev->devh);
out:
	libusb_exit(dev->ctx);
	return r;
}
