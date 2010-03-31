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
const unsigned int FRAME_SIZE = 292;
const unsigned int N_FRAMES = 16;

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

	/* buffer to hold raw image data frames */
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

static int dump_buffer (unsigned char *data, int length, unsigned char *prefix)
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
	d += dump_buffer(d,  2, "  Line type       ");
	d += dump_buffer(d,  2, "  Sequence        ");
	d += dump_buffer(d,  2, "  ???             ");
	fprintf(stdout, "\n");

	d += dump_buffer(d, 16, "  Fingerprint A   ");
	for (i=1; i<12; i++)
		d += dump_buffer(d, 16, "                  ");
	d += dump_buffer(d,  8, "                  ");
	fprintf(stdout, "\n");

	d += dump_buffer(d,  2, "  ???             ");
	fprintf(stdout, "\n");

	d += dump_buffer(d, 16, "  IMG B           ");
	d += dump_buffer(d, 16, "                  ");
	d += dump_buffer(d,  6, "                  ");
	d += dump_buffer(d, 16, "  IMG C           ");
	d += dump_buffer(d, 10, "                  ");
	fprintf(stdout, "\n");

	d += dump_buffer(d,  4, "  Constant        ");
	d += dump_buffer(d,  2, "  Sequence'       ");
	fprintf(stdout, "\n");

	d += dump_buffer(d,  1, "  S_curr_state    ");
	d += dump_buffer(d,  1, "  S_next_state    ");
	d += dump_buffer(d,  2, "  S_count         ");
	d += dump_buffer(d,  2, "  S_level         ");
	d += dump_buffer(d,  8, "  ???             ");
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
	if (length < FRAME_SIZE) {
		fprintf(stdout, "*** Short frame, dumping as %d raw bytes!!\n", length);
		dump_buffer(data, length, "");
		return skip + length;
	}

	// do the actual printing
	dump_frame_1(data, n);

	// return number of bytes consumed
	return skip + FRAME_SIZE;
}

static void dump_image (struct vfs_dev *dev)
{
	int f = 0;
	unsigned char *data = dev->ibuf;
	int length = dev->ilen;

	fprintf(stdout, "  %d frames in %d bytes%s\n", length/FRAME_SIZE, length, (length%FRAME_SIZE) ? " (incomplete frames(s)?)" : "");
	fprintf(stdout, "  {\n");
	while (length > 0) {
		int n = dump_frame(data, length, f++);
		data += n;
		length -= n;
	}
	fprintf(stdout, "  }\n");
}


/******************************************************************************************************
 * PNM formatter framework
 */

struct pnm_context;

typedef void (*pnm_func)   (struct pnm_context *, int, int);
typedef void (*pnm_func_1) (struct pnm_context *, int, int, int);

struct pnm_formatter {

	/* how many pixels are added on each side of image */
	int y0;   // top
	int y1;   // bottom
	int x0;   // left
	int x1;   // right

	/* printers for various areas */
	pnm_func   header;
	pnm_func_1 left;
	pnm_func   body;
	pnm_func_1 right;
	pnm_func   footer;
};

struct pnm_context {

	/* vfs_dev to create PNM from */
	struct vfs_dev *dev;

	/* file to print PNM into */
	FILE *file;

	/* which PNM formatter to use */
	struct pnm_formatter *fmt;

	/* how many scan lines are there */
	int height;

	/* first column of image stripe */
	int offset;

	/* width of image stripe */
	int len;
};

/* fill area with black */
static void _pnm_black (struct pnm_context *c, int y, int yy, int n)
{
	while (n--)
		fprintf(c->file, "   0");
}

/* fill area with vertical gradient */
static void _pnm_gradient (struct pnm_context *c, int y, int yy, int n)
{
	int z = 255*((float)y/(float)yy);
	while (n--)
		fprintf(c->file, " %3d", z);
}

/* fill area with 10 pixel ruler */
static void _pnm_ruler (struct pnm_context *c, int y, int yy, int n)
{
	int z = (y%10) ? 0 : 255;
	fprintf(c->file, " 128");
	n--;
	while (n--)
		fprintf(c->file, " %3d", z ? (n ? z : 128) : 128);
}

/* fill area with raw image data */
static void _pnm_frame (struct pnm_context *c, int y, int yy)
{
	unsigned char *data = c->dev->ibuf + y * FRAME_SIZE + c->offset;
	int i = c->len;
	while (i--)
		fprintf(c->file, " % 3d", *data++);
}

/* file area with image ABCD ruler */
static void _pnm_frameruler (struct pnm_context *c, int y, int yy)
{
	int offset = c->offset;
	int len = c->len;

	while (len--) {
		switch (offset++) {
		case 0:
		case 206:
		case 246:
		case 272:
			fprintf(c->file, ((y==yy-1) || (y==0)) ? " 128" : " 255");
			break;
		default:
			fprintf(c->file, " 128");
			break;
		}
	}
}

/* fill area with finger detection data */
static void _pnm_sense (struct pnm_context *c, int y, int yy, int n)
{
	unsigned char *data = c->dev->ibuf + y * FRAME_SIZE;
	int j = xx(data[283],data[282])>>2;
	if (data[1] == 0x01) j = 0;
	while (n--)
		fprintf(c->file, " % 3d", (j>255) ? ((n&y&1) ? 255 : 0) : j);
}

/* newline in the output pnm file */
static void _pnm_newline (struct pnm_context *c)
{
	fprintf(c->file, "\n");
}

/* create a PNM header for the output file */
static void _pnm_header (struct pnm_context *c)
{
	struct pnm_formatter *f = c->fmt;
	int n_x = c->len + f->x0 + f->x1;
	int n_y = c->height + f->y0 + f->y1;
	fprintf(c->file, "P2\n%d %d\n256\n", n_x, n_y);
}

/* call the three printers for each row of the section */
static void _pnm_section (struct pnm_context *c, int y, pnm_func_1 l, pnm_func m, pnm_func_1 r)
{
	struct pnm_formatter *f = c->fmt;
	int i;
	for (i=0; i<y; i++) {
		if (l) l (c, i, y, f->x0);
		if (m) m (c, i, y);
		if (r) r (c, i, y, f->x1);
		_pnm_newline(c);
	}
}

/* create a pnm file as a set of discrete areas */
static void show_pnm_1 (struct pnm_context *c)
{
	struct pnm_formatter *f = c->fmt;
	_pnm_header  (c);
	_pnm_section (c, f->y0,     _pnm_black, f->header, _pnm_black);
	_pnm_section (c, c->height, f->left,    f->body,   f->right);
	_pnm_section (c, f->y1,     _pnm_black, f->footer, _pnm_black);
}

/* open a new file and invoke the pnm creator */
static void show_pnm (struct vfs_dev *dev, unsigned char dir, int offset, int len, struct pnm_formatter *fmt)
{
	struct pnm_context _c, *c = &_c;
	char name[40];

	sprintf(name, "img/%c/out-%03d-%02x.pnm", dir, dev->inum, dev->inum);

	c->dev = dev;
	c->fmt = fmt;
	c->offset = offset;
	c->len = len;
	c->height = dev->ilen/FRAME_SIZE;
	c->file = fopen(name, "w");

	if (c->file != NULL) {
		show_pnm_1(c);
		fclose(c->file);

	} else {
		fprintf(stderr, "Can't open \"%s\" for writing", name);
	}
}



/******************************************************************************************************
 * Specific PNM formatters
 */

static struct pnm_formatter foo =
{
	.y0     = 5,
	.y1     = 5,
	.x0     = 40,
	.x1     = 5,
	.header = _pnm_frameruler,
	.left   = _pnm_sense,
	.body   = _pnm_frame,
	.right  = _pnm_ruler,
	.footer = _pnm_frameruler,
};

static struct pnm_formatter bar =
{
	.y0     = 0,
	.y1     = 0,
	.x0     = 0,
	.x1     = 0,
	.header = NULL,
	.left   = NULL,
	.body   = _pnm_frame,
	.right  = NULL,
	.footer = NULL,
};




static void create_pnms (struct vfs_dev *dev)
{
	if (dev->anonymous) return;
	show_pnm (dev, 'X',   0, 292, &foo);
	show_pnm (dev, 'Y',   0, 292, &bar);
	// show_pnm (dev, 'A',   0, 206, &foo);
	// show_pnm (dev, 'B', 206,  66, &foo);
	// show_pnm (dev, 'C', 272,  20, &foo);
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

	dump_buffer(data, len, "  --->");
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

	dump_buffer(dev->buf, dev->len, "  <---");
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
		int r = libusb_bulk_transfer(dev->devh, EP_IN(2), buf, N_FRAMES*FRAME_SIZE, &n, BULK_TIMEOUT);

		buf += n;
		*len += n;

		if (r < 0 && r != -7) {
			//fp_err("bulk read error %d", r);
			return r;
		}

	} while (n == N_FRAMES*FRAME_SIZE);

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

     01 - Reset
     02 - GetVersion
     03 - GetPrint
     04 - GetParam
     05 - SetParam
     06 - GetConfig
     07   DownloadPatch
     08   GetRateData
     09   IspRequest
     0A   ProgramFlash
     0B   EraseFlash
     0C   LedStates
     0D   LedEvent
     0E - AbortPrint
     0F   Spare2
     10   Spare3
     11   Spare4
     12 - Peek
     13 - Poke
     14 - SensorSpiTrans
     15   SensorGPIO
     16 - GetFingerState 
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
	if ((r == 0) && (!dev->anonymous)) {
		dump_image(dev);
		create_pnms(dev);
	}
	return r;
}

#undef _


/******************************************************************************************************
 * Parameters and registers
 */

const unsigned int P_MESS_WITH_BC      = 0x000c;
const unsigned int P_THRESHOLD         = 0x0057;
const unsigned int P_STATE_3_COUNT     = 0x005e;
const unsigned int P_STATE_5_COUNT     = 0x005f;
const unsigned int P_INFO_LINE_RATE    = 0x0062;
const unsigned int P_INFO_CONTRAST     = 0x0077;

const unsigned int VFS_EXPOSURE        = 0x00ff500e;
const unsigned int VFS_DARKEN_CD_1     = 0x00ff502c;
const unsigned int VFS_DARKEN_CD_2     = 0x00ff502e;
const unsigned int VFS_IMAGE_ABCD      = 0x00ff5032;
const unsigned int VFS_CONTRAST        = 0x00ff5038;
const unsigned int VFS_GRATING         = 0x00ff503e;
const unsigned int VFS_KILL_4          = 0x00ff9802;

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
		dump_buffer(r->data, r->len, "  XXXX            ");
	}
}


/******************************************************************************************************
 * raw terminal support
 */
#include <termios.h> 
 
static void raw (void) 
{ 
	struct termios tio; 
	tcgetattr( 0, &tio ); 
	tio.c_lflag &= ~ICANON; 
	tio.c_cc[VMIN] = 0;
	tio.c_cc[VTIME] = 1;
	tcsetattr( 0, TCSANOW, &tio ); 
} 
 
static void noraw (void) 
{ 
	struct termios tio; 
	tcgetattr( 0, &tio ); 
	tio.c_lflag |= ICANON; 
	tcsetattr( 0, TCSANOW, &tio ); 
} 
 
static int getch (void)
{
	char c;
	int n;
	n = read(0, &c, 1);
	return (n<0) ? n : (n==0) ? 0 : c;
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
	int done = 0;
	int x = 0x00ff8000;
	int w = 1;
	raw();
	while (!done) {
		int n = getch();
		if (n < 0)
			break;
		switch (n) {
		case 'p':
			done = 1;
			break;
		case 'z':
			x -= w;
			break;
		case 'x':
			x += w;
			break;
		case 'a':
			x -= 0x10;
			break;
		case 's':
			x += 0x10;
			break;
		case 'q':
			x -= 0x100;
			break;
		case 'w':
			x += 0x100;
			break;
		case '1':
			w = 1;
			break;
		case '2':
			w = 2;
			break;
		case '4':
			w = 4;
			break;
		}
		_(  Peek (dev, x, w));
		//usleep(50000);
	}
	noraw();
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
static int mess_with_bc = 0x010c;

/* The frequency of info lines */
static int info_line_rate = 0x32;

/* The best contrast value tried so far */
static int best_contrast = 0x00;

/* Image line exposure level */
static int exposure = 0x21bc;

/* Try a contrast register setting */
static int try_contrast (struct vfs_dev *dev, int value)
{
	_(  Poke (dev, VFS_CONTRAST, value, 0x01));
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
