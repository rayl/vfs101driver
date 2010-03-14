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
};


/******************************************************************************************************
 * Functions to deal with byte swapping.
 */
static inline unsigned char lo (int n)
{
	return n & 0xff;
}

static inline unsigned char hi (int n)
{
	return (n>>8) & 0xff;
}

static inline unsigned short xx (int h, int l)
{
	return (h<<8)|l;
}


/******************************************************************************************************
 * Debug printing routines
 */

static void dump_packet (unsigned char *data, int length, unsigned char *prefix)
{
	int i = 0;
	fprintf(stdout, "%s ", prefix);
	for (i; i < length; i++)
		fprintf(stdout, "0x%02X ", data[i]);
	fprintf(stdout, "\n");
}

static void dump_image (unsigned char *data, int length)
{
	int i;

	fprintf(stdout, "Image data, %d bytes%s\n", length, (length%PKTSIZE) ? " (incomplete packet(s)?)" : "");
	for (i = 0; i < length; i++){
		fprintf(stdout, "%02X ", data[i]);
		if ((i & 15) == 15){
			int z;
			fprintf(stdout, "                       ");
			for (z = i - 16; z < i; z++)
				fprintf(stdout, "%c", data[z]);
			fprintf(stdout, "\n");
		}
	}
	if (i & 15)
		fprintf(stdout, "\n");
	fprintf(stdout, "\n");
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

	data[0] = lo(dev->seq);
	data[1] = hi(dev->seq);

	dump_packet(data, len, "--->");
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

	dump_packet(dev->buf, dev->len, "<---");
	if ((lo(dev->seq) != dev->buf[0]) || (hi(dev->seq) != dev->buf[1])) {
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
     00 00 06 00    - GetConfiguration
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
     00 00 13 00      Poke
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
static int GetPrint (struct vfs_dev *dev, int count, unsigned char args[6])
{
	unsigned char q1[0x0e] = { 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
	int i;
	q1[6] = lo(count);
	q1[7] = hi(count);
	for (i=0; i<6; i++) q1[8+i] = args[i];
	_();
	return swap (dev, q1, 0x0e);
}

/* GetParam (00 00 04 00)
 *
 *  Retrieve a parameter value from the device.
 */
static int GetParam (struct vfs_dev *dev, int param)
{
	unsigned char q1[0x08] = { 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00 };
	q1[6] = lo(param);
	q1[7] = hi(param);
	_();
	return swap (dev, q1, 0x08);
}

/* SetParam (00 00 05 00)
 *
 *  Set a parameter value on the device.
 */
static int SetParam (struct vfs_dev *dev, int param, int value)
{
	unsigned char q1[0x0a] = { 0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00 };
	q1[6] = lo(param);
	q1[7] = hi(param);
	q1[8] = lo(value);
	q1[9] = hi(value);
	_();
	return swap (dev, q1, 0x0a);
}

/* GetConfiguration (00 00 06 00)
 *
 *  Retrieve config info from the device.
 */
static int GetConfiguration (struct vfs_dev *dev)
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
static int Peek (struct vfs_dev *dev, int p1, int p2, int p3, int p4, int p5)
{
	unsigned char q1[0x0b] = { 0x00, 0x00, 0x00, 0x00, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
	q1[6] = p1;
	q1[7] = p2;
	q1[8] = p3;
	q1[9] = p4;
	q1[10] = p5;
	_();
	return swap (dev, q1, 0x0b);
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
	if (r == 0)
		dump_image(dev->ibuf, dev->ilen);
	return r;
}


/******************************************************************************************************
 * Stuff
 */


static int validity_cycle4(struct vfs_dev *dev){
	usleep(100000);
	Peek(dev, 0xE8, 0x1F, 0x00, 0x00, 0x04);
	Peek(dev, 0xEC, 0x1F, 0x00, 0x00, 0x04);
	Peek(dev, 0xF0, 0x1F, 0x00, 0x00, 0x04);
	Peek(dev, 0xF4, 0x1F, 0x00, 0x00, 0x04);
	Peek(dev, 0xF8, 0x1F, 0x00, 0x00, 0x04);
	Peek(dev, 0xFC, 0x1F, 0x00, 0x00, 0x04);
	GetParam(dev, 0x2e);
	GetVersion(dev);
	GetParam(dev, 0x28);
	GetParam(dev, 0x14);
}

static int validity_cycle3(struct vfs_dev *dev){
	unsigned char data16[6]  = "\x00\x01\x00\x00\x00\x01";
	unsigned char data98[6]  = "\x00\x00\x00\x00\x00\x01";
	unsigned char data109[6] = "\x00\x01\x00\x00\x00\x01";
	unsigned char data116[6] = "\x00\x01\x00\x00\x00\x01";
	unsigned char data127[6] = "\x00\x01\x00\x00\x00\x01";
	unsigned char data135[6] = "\x00\x01\x00\x00\x00\x01";
	unsigned char data137[6] = "\x00\x01\x00\x00\x00\x01";
	unsigned char data139[6] = "\x00\x01\x00\x00\x00\x01";
	unsigned char data141[6] = "\x00\x01\x00\x00\x00\x01";
	unsigned char data143[6] = "\x00\x01\x00\x00\x00\x01";
	unsigned char data145[6] = "\x00\x01\x00\x00\x00\x01";
	unsigned char data147[6] = "\x00\x01\x00\x00\x00\x01";
	unsigned char data149[6] = "\x00\x01\x00\x00\x00\x01";
	unsigned char data157[6] = "\x00\x01\x00\x00\x00\x01";
	unsigned char data170[6] = "\x00\x01\x00\x00\x00\x01";
	unsigned char data176[6] = "\x01\x00\x00\x00\x01\x01";

	unsigned char data89[] = "\x59\x00\x00\x00\x14\x00\x05\x00\xAB\x00\x00\x00\x00";
	unsigned char data102[] = "\x66\x00\x00\x00\x13\x00\xF6\x05\x00\x00\x01\x00\x00\x00\x01";
	unsigned char data104[] = "\x68\x00\x00\x00\x13\x00\x3E\x50\xFF\x00\x00\x00\x00\x00\x01";
	unsigned char data108[] = "\x6C\x00\x00\x00\x13\x00\x06\x98\xFF\x00\x00\x00\x00\x00\x01";
	unsigned char data110[] = "\x6E\x00\x00\x00\x13\x00\xF6\x05\x00\x00\x00\x00\x00\x00\x01";
	unsigned char data112[] = "\x70\x00\x00\x00\x13\x00\x3E\x50\xFF\x00\x10\x00\x00\x00\x01";
	unsigned char data120[] = "\x78\x00\x00\x00\x13\x00\x32\x50\xFF\x00\x12\x00\x00\x00\x01";
	unsigned char data121[] = "\x79\x00\x00\x00\x13\x00\x0E\x50\xFF\x00\x00\x40\x00\x00\x02";
	unsigned char data122[] = "\x7A\x00\x00\x00\x13\x00\x38\x50\xFF\x00\x0F\x00\x00\x00\x01";
	unsigned char data128[] = "\x80\x00\x00\x00\x13\x00\xF6\x05\x00\x00\x01\x00\x00\x00\x01";
	unsigned char data130[] = "\x82\x00\x00\x00\x13\x00\x3E\x50\xFF\x00\x00\x00\x00\x00\x01";
	unsigned char data134[] = "\x86\x00\x00\x00\x13\x00\x38\x50\xFF\x00\x0E\x00\x00\x00\x01";
	unsigned char data136[] = "\x88\x00\x00\x00\x13\x00\x38\x50\xFF\x00\x0D\x00\x00\x00\x01";
	unsigned char data138[] = "\x8A\x00\x00\x00\x13\x00\x38\x50\xFF\x00\x0C\x00\x00\x00\x01";
	unsigned char data140[] = "\x8C\x00\x00\x00\x13\x00\x38\x50\xFF\x00\x0B\x00\x00\x00\x01";
	unsigned char data142[] = "\x8E\x00\x00\x00\x13\x00\x38\x50\xFF\x00\x0A\x00\x00\x00\x01";
	unsigned char data144[] = "\x90\x00\x00\x00\x13\x00\x38\x50\xFF\x00\x09\x00\x00\x00\x01";
	unsigned char data146[] = "\x92\x00\x00\x00\x13\x00\x38\x50\xFF\x00\x08\x00\x00\x00\x01";
	unsigned char data148[] = "\x94\x00\x00\x00\x13\x00\x38\x50\xFF\x00\x07\x00\x00\x00\x01";
	unsigned char data150[] = "\x96\x00\x00\x00\x13\x00\xF6\x05\x00\x00\x00\x00\x00\x00\x01";
	unsigned char data152[] = "\x98\x00\x00\x00\x13\x00\x3E\x50\xFF\x00\x10\x00\x00\x00\x01";
	unsigned char data156[] = "\x9C\x00\x00\x00\x13\x00\x06\x98\xFF\x00\x00\x00\x00\x00\x01";
	unsigned char data161[] = "\xA1\x00\x00\x00\x13\x00\x38\x50\xFF\x00\x14\x00\x00\x00\x01";
	unsigned char data162[] = "\xA2\x00\x00\x00\x13\x00\x0E\x50\xFF\x00\xB4\x21\x00\x00\x02";
	unsigned char data163[] = "\xA3\x00\x00\x00\x13\x00\x32\x50\xFF\x00\x31\x00\x00\x00\x01";
        int r = 0;
	Peek(dev, 0xE8, 0x1F, 0x00, 0x00, 0x04);
	Peek(dev, 0xEC, 0x1F, 0x00, 0x00, 0x04);
	Peek(dev, 0xF0, 0x1F, 0x00, 0x00, 0x04);
	Peek(dev, 0xF4, 0x1F, 0x00, 0x00, 0x04);
	Peek(dev, 0xF8, 0x1F, 0x00, 0x00, 0x04);
	Peek(dev, 0xFC, 0x1F, 0x00, 0x00, 0x04);
	GetParam(dev, 0x2e);
	GetVersion(dev);
	GetParam(dev, 0x28);
	GetParam(dev, 0x14);
	GetParam(dev, 0x11);
	GetParam(dev, 0x54);
	GetParam(dev, 0x01);
	GetParam(dev, 0x14);
	AbortPrint(dev);
	LoadImage(dev);
	LoadImage(dev);
	GetPrint(dev, 1, data16);
	AbortPrint(dev);
	SetParam(dev, 0x0004, 0x0000);
	SetParam(dev, 0x0005, 0x0000);
	SetParam(dev, 0x0006, 0x0000);
	SetParam(dev, 0x0007, 0x0000);
	SetParam(dev, 0x000A, 0x0002);
	SetParam(dev, 0x000B, 0x010B);
	SetParam(dev, 0x000C, 0x010C);
	SetParam(dev, 0x000D, 0x010D);
	SetParam(dev, 0x000E, 0x0001);
	SetParam(dev, 0x0010, 0x0000);
	SetParam(dev, 0x0012, 0x0040);
	SetParam(dev, 0x0015, 0x04B0);
	SetParam(dev, 0x0016, 0x001B);
	SetParam(dev, 0x0017, 0x0001);
	SetParam(dev, 0x0018, 0x0003);
	SetParam(dev, 0x0019, 0x0001);
	SetParam(dev, 0x001A, 0x0000);
	SetParam(dev, 0x001B, 0x0004);
	SetParam(dev, 0x001D, 0x010C);
	SetParam(dev, 0x001E, 0x010D);
	SetParam(dev, 0x001F, 0x0000);
	SetParam(dev, 0x0020, 0x0000);
	SetParam(dev, 0x0021, 0x0001);
	SetParam(dev, 0x0022, 0x0001);
	SetParam(dev, 0x0023, 0x0001);
	SetParam(dev, 0x0024, 0x0000);
	SetParam(dev, 0x002C, 0x0046);
	SetParam(dev, 0x002D, 0x002A);
	SetParam(dev, 0x002F, 0x010E);
	SetParam(dev, 0x0030, 0x010F);
	SetParam(dev, 0x0032, 0x0004);
	SetParam(dev, 0x0033, 0x0003);
	SetParam(dev, 0x0034, 0x0005);
	SetParam(dev, 0x0035, 0x0010);
	SetParam(dev, 0x0036, 0x000A);
	SetParam(dev, 0x0037, 0x0018);
	SetParam(dev, 0x0038, 0x001E);
	SetParam(dev, 0x0039, 0x00B2);
	SetParam(dev, 0x003A, 0x0080);
	SetParam(dev, 0x003B, 0x0000);
	SetParam(dev, 0x003E, 0x01F4);
	SetParam(dev, 0x003F, 0x001A);
	SetParam(dev, 0x0040, 0x00F8);
	SetParam(dev, 0x0042, 0x0001);
	SetParam(dev, 0x0043, 0x0004);
	SetParam(dev, 0x0044, 0x0010);
	SetParam(dev, 0x0045, 0x0005);
	SetParam(dev, 0x0046, 0x00F5);
	SetParam(dev, 0x0047, 0x000C);
	SetParam(dev, 0x0048, 0x0000);
	SetParam(dev, 0x0049, 0x0000);
	SetParam(dev, 0x0053, 0x0000);
	SetParam(dev, 0x0056, 0x00B4);
	SetParam(dev, 0x0057, 0x0096);
	SetParam(dev, 0x0058, 0x008C);
	SetParam(dev, 0x0059, 0x0064);
	SetParam(dev, 0x005B, 0x0001);
	SetParam(dev, 0x005C, 0x0001);
	SetParam(dev, 0x005D, 0x0020);
	SetParam(dev, 0x005E, 0x0064);
	SetParam(dev, 0x005F, 0x00C8);
	SetParam(dev, 0x0060, 0x00C8);
	SetParam(dev, 0x0062, 0x0000);
	SetParam(dev, 0x0064, 0x011A);
	SetParam(dev, 0x0069, 0x0014);
	GetParam(dev, 0x2a);
	GetParam(dev, 0x3c);
	GetParam(dev, 0x4a);
	GetParam(dev, 0x41);
	GetVersion(dev);
	SetParam(dev, 0x0063, 0x0001);
	swap(dev, data89, (int) sizeof(data89) - 1); 
	SetParam(dev, 0x0064, 0x0118);
	GetConfiguration(dev);
	SetParam(dev, 0x0046, 0x00F5);
	SetParam(dev, 0x0055, 0x0008);
	SetParam(dev, 0x006D, 0x0032);
	SetParam(dev, 0x006E, 0x0003);
	GetParam(dev, 0x52);
	SetParam(dev, 0x0052, 0x0320);
	GetPrint(dev, 1, data98);
	LoadImage(dev);
	SetParam(dev, 0x0052, 0x1EB4);
	Peek(dev, 0x2C, 0x50, 0xFF, 0x00, 0x02);
	Peek(dev, 0x2E, 0x50, 0xFF, 0x00, 0x02);
	swap(dev, data102, (int) sizeof(data102) - 1); 
	Peek(dev, 0x3E, 0x50, 0xFF, 0x00, 0x01);
	swap(dev, data104, (int) sizeof(data104) - 1); 
	Peek(dev, 0x02, 0x98, 0xFF, 0x00, 0x01);
	Peek(dev, 0x00, 0x98, 0xFF, 0x00, 0x01);
	Peek(dev, 0x06, 0x98, 0xFF, 0x00, 0x01);
	swap(dev, data108, (int) sizeof(data108) - 1); 
	GetPrint(dev, 100, data109);
	LoadImage(dev);
	swap(dev, data110, (int) sizeof(data110) - 1); 
	Peek(dev, 0x3E, 0x50, 0xFF, 0x00, 0x01);
	swap(dev, data112, (int) sizeof(data112) - 1); 
	Peek(dev, 0x02, 0x98, 0xFF, 0x00, 0x01);
	Peek(dev, 0x00, 0x98, 0xFF, 0x00, 0x01);
	Peek(dev, 0x06, 0x98, 0xFF, 0x00, 0x01);
	GetPrint(dev, 100, data116);
	LoadImage(dev);
	Peek(dev, 0x38, 0x50, 0xFF, 0x00, 0x01);
	Peek(dev, 0x0E, 0x50, 0xFF, 0x00, 0x02);
	Peek(dev, 0x32, 0x50, 0xFF, 0x00, 0x01);
	swap(dev, data120, (int) sizeof(data120) - 1); 
	swap(dev, data121, (int) sizeof(data121) - 1); 
	swap(dev, data122, (int) sizeof(data122) - 1); 
	SetParam(dev, 0x0062, 0x0000);
	SetParam(dev, 0x0077, 0x0000);
	SetParam(dev, 0x0076, 0x0000);
	SetParam(dev, 0x0078, 0x0000);
	GetPrint(dev, 2, data127);
	LoadImage(dev);
	swap(dev, data128, (int) sizeof(data128) - 1); 
	Peek(dev, 0x3E, 0x50, 0xFF, 0x00, 0x01);
	swap(dev, data130, (int) sizeof(data130) - 1); 
	Peek(dev, 0x02, 0x98, 0xFF, 0x00, 0x01);
	Peek(dev, 0x00, 0x98, 0xFF, 0x00, 0x01);
	Peek(dev, 0x06, 0x98, 0xFF, 0x00, 0x01);
	swap(dev, data134, (int) sizeof(data134) - 1); 
	GetPrint(dev, 10, data135);
	LoadImage(dev);
	swap(dev, data136, (int) sizeof(data136) - 1); 
	GetPrint(dev, 10, data137);
	LoadImage(dev);
	swap(dev, data138, (int) sizeof(data138) - 1); 
	GetPrint(dev, 10, data139);
	LoadImage(dev);
	swap(dev, data140, (int) sizeof(data140) - 1); 
	GetPrint(dev, 10, data141);
	LoadImage(dev);
	swap(dev, data142, (int) sizeof(data142) - 1); 
	GetPrint(dev, 10, data143);
	LoadImage(dev);
	swap(dev, data144, (int) sizeof(data144) - 1); 
	GetPrint(dev, 10, data145);
	LoadImage(dev);
	swap(dev, data146, (int) sizeof(data146) - 1); 
	GetPrint(dev, 10, data147);
	LoadImage(dev);
	swap(dev, data148, (int) sizeof(data148) - 1); 
	GetPrint(dev, 10, data149);
	LoadImage(dev);
	swap(dev, data150, (int) sizeof(data150) - 1); 
	Peek(dev, 0x3E, 0x50, 0xFF, 0x00, 0x01);
	swap(dev, data152, (int) sizeof(data152) - 1); 
	Peek(dev, 0x02, 0x98, 0xFF, 0x00, 0x01);
	Peek(dev, 0x00, 0x98, 0xFF, 0x00, 0x01);
	Peek(dev, 0x06, 0x98, 0xFF, 0x00, 0x01);
	swap(dev, data156, (int) sizeof(data156) - 1); 
	GetPrint(dev, 10, data157);
	LoadImage(dev);
	SetParam(dev, 0x0077, 0x0007);
	SetParam(dev, 0x0076, 0x0012);
	SetParam(dev, 0x0078, 0x21A0);
	swap(dev, data161, (int) sizeof(data161) - 1); 
	swap(dev, data162, (int) sizeof(data162) - 1); 
	swap(dev, data163, (int) sizeof(data163) - 1); 
	SetParam(dev, 0x0062, 0x0032);
	AbortPrint(dev);
	LoadImage(dev);
	LoadImage(dev);
	SetParam(dev, 0x0062, 0x0032);
	GetParam(dev, 0x14);
	GetParam(dev, 0x11);
	SetParam(dev, 0x0062, 0x0032);
	GetPrint(dev, 20, data170);
	LoadImage(dev);
	GetParam(dev, 0x14);
	GetParam(dev, 0x14);
	AbortPrint(dev);
	LoadImage(dev);
	LoadImage(dev);
	GetParam(dev, 0x11);
	SetParam(dev, 0x0062, 0x0032);
	GetPrint(dev, 5000, data176);

	int i = 0;
	for (i; i < 50; i++)
		GetFingerState(dev);
		usleep(750000);
	return 0;
}

static int validity_cycle2(struct vfs_dev *dev){
	Peek(dev, 0xE8, 0x1F, 0x00, 0x00, 0x04);
	Peek(dev, 0xEC, 0x1F, 0x00, 0x00, 0x04);
	Peek(dev, 0xF0, 0x1F, 0x00, 0x00, 0x04);
	Peek(dev, 0xF4, 0x1F, 0x00, 0x00, 0x04);
	Peek(dev, 0xF8, 0x1F, 0x00, 0x00, 0x04);
	Peek(dev, 0xFC, 0x1F, 0x00, 0x00, 0x04);
	return GetFingerState(dev);
}

static int validity_cycle1(struct vfs_dev *dev){
	unsigned char data15[6] = "\x00\x01\x00\x00\x00\x01";
	unsigned char data37[6] = "\x01\x00\x00\x00\x01\x01";
	int r;

	r = Peek(dev, 0xE8, 0x1F, 0x00, 0x00, 0x04);
	if (r != 0)                                               
		return r;                                         
	r = Peek(dev, 0xEC, 0x1F, 0x00, 0x00, 0x04);
	if (r != 0)                                               
		return r;                                         
	r = Peek(dev, 0xF0, 0x1F, 0x00, 0x00, 0x04);
	if (r != 0)                                               
		return r;                                         
	r = Peek(dev, 0xF4, 0x1F, 0x00, 0x00, 0x04);
	if (r != 0)                                               
		return r;                                         
	r = Peek(dev, 0xF8, 0x1F, 0x00, 0x00, 0x04);
	if (r != 0)                                               
		return r;                                         
	r = Peek(dev, 0xFC, 0x1F, 0x00, 0x00, 0x04);
	if (r != 0)                                               
		return r;                                         
	r = GetParam(dev, 0x2e);
	if (r != 0)                                               
		return r;                                         
	r = GetVersion(dev);
	if (r != 0)                                               
		return r;                                         
	r = GetParam(dev, 0x28);
	if (r != 0)                                               
		return r;                                         
	r = GetParam(dev, 0x14);
	if (r != 0)                                               
		return r;                                         
	r = GetParam(dev, 0x11);
	if (r != 0)                                               
		return r;                                         
	r = GetParam(dev, 0x54);
	if (r != 0)                                               
		return r;                                         
	r = GetParam(dev, 0x01);
	if (r != 0)                                               
		return r;                                         
	r = GetParam(dev, 0x14);
	if (r != 0)                                               
		return r;                                         
	r = AbortPrint(dev);
	if (r != 0)                                               
		return r;                                         
	r = GetPrint(dev, 1, data15);
	if (r != 0)                                               
		return r;                                         

	LoadImage(dev);
	LoadImage(dev);

	r = AbortPrint(dev);
	if (r != 0)                                               
		return r;                                         

	LoadImage(dev);
	
	r = SetParam(dev, 0x0004, 0x0000);
	if (r != 0)                                               
		return r;                                         

	LoadImage(dev);
	LoadImage(dev);

	r = SetParam(dev, 0x0005, 0x0000);
	if (r != 0)                                               
		return r;                                         
	r = SetParam(dev, 0x0006, 0x0000);
	if (r != 0)                                               
		return r;                                         
	r = SetParam(dev, 0x0007, 0x0000);
	if (r != 0)                                               
		return r;                                         
	r = SetParam(dev, 0x000A, 0x0002);
	if (r != 0)                                               
		return r;                                         
	r = SetParam(dev, 0x000B, 0x010B);
	if (r != 0)                                               
		return r;                                         
	r = SetParam(dev, 0x000C, 0x010C);
	if (r != 0)                                               
		return r;                                         
	r = SetParam(dev, 0x000D, 0x010D);
	if (r != 0)                                               
		return r;                                         
	r = SetParam(dev, 0x000E, 0x0001);
	if (r != 0)                                               
		return r;                                         
	r = SetParam(dev, 0x0010, 0x0000);
	if (r != 0)                                               
		return r;                                         
	r = SetParam(dev, 0x0012, 0x0040);
	if (r != 0)                                               
		return r;                                         
	r = SetParam(dev, 0x0015, 0x04B0);
	if (r != 0)                                               
		return r;                                         
	r = SetParam(dev, 0x0016, 0x001B);
	if (r != 0)                                               
		return r;                                         
	r = SetParam(dev, 0x0017, 0x0001);
	if (r != 0)                                               
		return r;                                         
	r = SetParam(dev, 0x0018, 0x0003);
	if (r != 0)                                               
		return r;                                         

	r = GetParam(dev, 0x14);
	if (r != 0)                                               
		return r;                                         
	r = GetParam(dev, 0x14);
	if (r != 0)                                               
		return r;                                         
	r = AbortPrint(dev);
	if (r != 0)                                               
		return r;                                         
	r = GetParam(dev, 0x11);
	if (r != 0)                                               
		return r;                                         
	r = SetParam(dev, 0x0062, 0x0032);
	if (r != 0)                                               
		return r;                                         
	r = GetPrint(dev, 5000, data37);
	if (r != 0)                                               
		return r;                                         

	int i = 0;
	for (i; i < 80; i++){
		r = GetFingerState(dev);
		if (r < 0)
			return r;
	}	

	return 0;
}

static int validity_cycle0(struct vfs_dev *dev){
//	unsigned char data171[6] = "\x00\x01\x00\x00\x00\x01";

	Peek(dev, 0xE8, 0x1F, 0x00, 0x00, 0x04);
	Peek(dev, 0xEC, 0x1F, 0x00, 0x00, 0x04);
	Peek(dev, 0xF0, 0x1F, 0x00, 0x00, 0x04);
	Peek(dev, 0xF4, 0x1F, 0x00, 0x00, 0x04);
	Peek(dev, 0xF8, 0x1F, 0x00, 0x00, 0x04);
	Peek(dev, 0xFC, 0x1F, 0x00, 0x00, 0x04);


	SetParam(dev, 0x0062, 0x0032);
//	GetPrint(dev, 20, data171);
//	LoadImage(dev)	;

	int i = 0;

	for (i; i < 10; i++){
		GetFingerState(dev);
	}

	return 0; 
}
	
static int validity_cycle(struct vfs_dev *dev){
	unsigned char data2[6] = "\x00\x01\x00\x00\x00\x01";
	unsigned char data6[6] = "\x01\x00\x00\x00\x01\x01"; 
	int r;

	r = Peek(dev, 0xE8, 0x1F, 0x00, 0x00, 0x04);
	if (r != 0)
		return r;
	r = Peek(dev, 0xEC, 0x1F, 0x00, 0x00, 0x04);
	if (r != 0)
		return r;
	r = GetPrint(dev, 1, data2);
	if (r != 0)
		return r;
	LoadImage(dev);

	r = AbortPrint(dev);
	if (r != 0)
		return r;

	r = GetParam(dev, 0x11);
	if (r != 0)
		return r;

	r = SetParam(dev, 0x0062, 0x0032); 
	if (r != 0)
		return r;

	r = GetPrint(dev, 5000, data2);
	if (r != 0)
		return r;

	int i = 0;
	for (i; i < 20; i++){
		r = GetFingerState(dev);
		if (r <= 0)
			return r;
	}	

	return 0;	
}

typedef int (*cycle_func)(struct vfs_dev *);

static cycle_func func (const char *id)
{
	if (id == NULL)           return validity_cycle;
	if (strcmp(id, "0") == 0) return validity_cycle0;
	if (strcmp(id, "1") == 0) return validity_cycle1;
	if (strcmp(id, "2") == 0) return validity_cycle2;
	if (strcmp(id, "3") == 0) return validity_cycle3;
	if (strcmp(id, "4") == 0) return validity_cycle4;
	                          return validity_cycle;
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
