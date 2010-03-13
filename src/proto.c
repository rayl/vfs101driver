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

/** Constants */
const unsigned char VALIDITY_RECEIVE_ENDPOINT_LONG = 0x82; 

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
	printf("%s ", prefix);
	for (i; i < length; i++)
		fprintf(stdout, "0x%02X ", data[i]);
	putchar('\n');
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
	printf("\n");

	if ((lo(dev->seq) != dev->buf[0]) || (hi(dev->seq) != dev->buf[1])) {
		fprintf(stderr, "Seqnum mismatch, got %04x, expected %04x", xx(dev->buf[1],dev->buf[0]), dev->seq);
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
     00 00 12 00      Peek
     00 00 13 00      Poke
     00 00 14 00      SensorSpiTrans
     00 00 15 00      SensorGPIO
     00 00 16 00    - GetFingerState 
*/

#define _() fprintf(stderr, "%s\n", __FUNCTION__)

/* Reset (00 00 01 00)
 *
 *  Cause the device to reenumerate on the USB bus.
 */
static void Reset (struct vfs_dev *dev)
{
	unsigned char q1[0x06] = { 0x00, 0x00, 0x00, 0x00, 0x01, 0x00 };
	_();
	swap (dev, q1, 0x06);
}

/* GetVersion (00 00 01 00)
 *
 *  Retrieve version string from the device.
 */
static void GetVersion (struct vfs_dev *dev)
{
	unsigned char q1[0x07] = { 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00 };
	_();
	swap (dev, q1, 0x07);
}

/* GetPrint (00 00 03 00)
 *
 *  Retrieve fingerprint image information.
 */
static void GetPrint (struct vfs_dev *dev, int count, unsigned char args[6])
{
	unsigned char q1[0x0e] = { 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
	int i;
	q1[6] = lo(count);
	q1[7] = hi(count);
	for (i=0; i<6; i++) q1[8+i] = args[i];
	_();
	swap (dev, q1, 0x0e);
}

/* GetParam (00 00 04 00)
 *
 *  Retrieve a parameter value from the device.
 */
static void GetParam (struct vfs_dev *dev, int param)
{
	unsigned char q1[0x08] = { 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00 };
	q1[6] = lo(param);
	q1[7] = hi(param);
	_();
	swap (dev, q1, 0x08);
}

/* SetParam (00 00 05 00)
 *
 *  Set a parameter value on the device.
 */
static void SetParam (struct vfs_dev *dev, int param, int value)
{
	unsigned char q1[0x0a] = { 0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00 };
	q1[6] = lo(param);
	q1[7] = hi(param);
	q1[8] = lo(value);
	q1[9] = hi(value);
	_();
	swap (dev, q1, 0x0a);
}

/* GetConfiguration (00 00 06 00)
 *
 *  Retrieve config info from the device.
 */
static void GetConfiguration (struct vfs_dev *dev)
{
	unsigned char q1[0x06] = { 0x00, 0x00, 0x00, 0x00, 0x06, 0x00 };
	_();
	swap (dev, q1, 0x06);
}

/* AbortPrint (00 00 0e 00)
 *
 *  Abort the current scan operation.
 */
static void AbortPrint (struct vfs_dev *dev)
{
	unsigned char q1[0x06] = { 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00 };
	_();
	swap (dev, q1, 0x06);
}

/* GetFingerState (00 00 16 00)
 *
 *  Poll device for current finger state.
 */
static int GetFingerState (struct vfs_dev *dev)
{
	unsigned char q1[0x06] = { 0x00, 0x00, 0x00, 0x00, 0x16, 0x00 };
	_();
	swap (dev, q1, 0x06);
	return dev->buf[0x0a];
}

static void LoadImage (struct vfs_dev *dev)
{
	load(dev, dev->ibuf, &dev->ilen);
	dump_image(dev->ibuf, dev->ilen);
}


/******************************************************************************************************
 * Stuff
 */


static int validity_receive_long_data(struct vfs_dev *dev){
	LoadImage(dev);
	return 0;
}

static int validity_swap_messages(struct vfs_dev *dev, unsigned char *data, int length){
	return swap(dev, data, length);
}

static int validity_cycle4(struct vfs_dev *dev){
	usleep(100000);
	unsigned char data1[] = "\x01\x00\x00\x00\x12\x00\xE8\x1F\x00\x00\x04";
	unsigned char data2[] = "\x02\x00\x00\x00\x12\x00\xEC\x1F\x00\x00\x04";
	unsigned char data3[] = "\x03\x00\x00\x00\x12\x00\xF0\x1F\x00\x00\x04";
	unsigned char data4[] = "\x04\x00\x00\x00\x12\x00\xF4\x1F\x00\x00\x04";
	unsigned char data5[] = "\x05\x00\x00\x00\x12\x00\xF8\x1F\x00\x00\x04";
	unsigned char data6[] = "\x06\x00\x00\x00\x12\x00\xFC\x1F\x00\x00\x04";
	unsigned char data7[] = "\x07\x00\x00\x00\x04\x00\x2E\x00";
	unsigned char data8[] = "\x08\x00\x00\x00\x02\x00\x00";
	unsigned char data9[] = "\x09\x00\x00\x00\x04\x00\x28\x00";
	unsigned char data10[] = "\x0A\x00\x00\x00\x04\x00\x14\x00";
        int r = 0;

	r = validity_swap_messages(dev, data1, (int) sizeof(data1) - 1); 
	validity_swap_messages(dev, data2, (int) sizeof(data2) - 1); 
	validity_swap_messages(dev, data3, (int) sizeof(data3) - 1); 
	validity_swap_messages(dev, data4, (int) sizeof(data4) - 1); 
	validity_swap_messages(dev, data5, (int) sizeof(data5) - 1); 
	validity_swap_messages(dev, data6, (int) sizeof(data6) - 1); 
	validity_swap_messages(dev, data7, (int) sizeof(data7) - 1); 
	validity_swap_messages(dev, data8, (int) sizeof(data8) - 1); 
	validity_swap_messages(dev, data9, (int) sizeof(data9) - 1); 
	validity_swap_messages(dev, data10, (int) sizeof(data10) - 1); 
	return r;
}

static int validity_cycle3(struct vfs_dev *dev){
	unsigned char data1[] = "\x01\x00\x00\x00\x12\x00\xE8\x1F\x00\x00\x04";
	unsigned char data2[] = "\x02\x00\x00\x00\x12\x00\xEC\x1F\x00\x00\x04";
	unsigned char data3[] = "\x03\x00\x00\x00\x12\x00\xF0\x1F\x00\x00\x04";
	unsigned char data4[] = "\x04\x00\x00\x00\x12\x00\xF4\x1F\x00\x00\x04";
	unsigned char data5[] = "\x05\x00\x00\x00\x12\x00\xF8\x1F\x00\x00\x04";
	unsigned char data6[] = "\x06\x00\x00\x00\x12\x00\xFC\x1F\x00\x00\x04";
	unsigned char data7[] = "\x07\x00\x00\x00\x04\x00\x2E\x00";
	unsigned char data8[] = "\x08\x00\x00\x00\x02\x00\x00";
	unsigned char data9[] = "\x09\x00\x00\x00\x04\x00\x28\x00";
	unsigned char data10[] = "\x0A\x00\x00\x00\x04\x00\x14\x00";
	unsigned char data11[] = "\x0B\x00\x00\x00\x04\x00\x11\x00";
	unsigned char data12[] = "\x0C\x00\x00\x00\x04\x00\x54\x00";
	unsigned char data13[] = "\x0D\x00\x00\x00\x04\x00\x01\x00";
	unsigned char data14[] = "\x0E\x00\x00\x00\x04\x00\x14\x00";
	unsigned char data15[] = "\x0F\x00\x00\x00\x0E\x00";
	unsigned char data16[] = "\x10\x00\x00\x00\x03\x00\x01\x00\x00\x01\x00\x00\x00\x01";
	unsigned char data17[] = "\x11\x00\x00\x00\x0E\x00";
	unsigned char data18[] = "\x12\x00\x00\x00\x05\x00\x04\x00\x00\x00";
	unsigned char data19[] = "\x13\x00\x00\x00\x05\x00\x05\x00\x00\x00";
	unsigned char data20[] = "\x14\x00\x00\x00\x05\x00\x06\x00\x00\x00";
	unsigned char data21[] = "\x15\x00\x00\x00\x05\x00\x07\x00\x00\x00";
	unsigned char data22[] = "\x16\x00\x00\x00\x05\x00\x0A\x00\x02\x00";
	unsigned char data23[] = "\x17\x00\x00\x00\x05\x00\x0B\x00\x0B\x01";
	unsigned char data24[] = "\x18\x00\x00\x00\x05\x00\x0C\x00\x0C\x01";
	unsigned char data25[] = "\x19\x00\x00\x00\x05\x00\x0D\x00\x0D\x01";
	unsigned char data26[] = "\x1A\x00\x00\x00\x05\x00\x0E\x00\x01\x00";
	unsigned char data27[] = "\x1B\x00\x00\x00\x05\x00\x10\x00\x00\x00";
	unsigned char data28[] = "\x1C\x00\x00\x00\x05\x00\x12\x00\x40\x00";
	unsigned char data29[] = "\x1D\x00\x00\x00\x05\x00\x15\x00\xB0\x04";
	unsigned char data30[] = "\x1E\x00\x00\x00\x05\x00\x16\x00\x1B\x00";
	unsigned char data31[] = "\x1F\x00\x00\x00\x05\x00\x17\x00\x01\x00";
	unsigned char data32[] = "\x20\x00\x00\x00\x05\x00\x18\x00\x03\x00";
	unsigned char data33[] = "\x21\x00\x00\x00\x05\x00\x19\x00\x01\x00";
	unsigned char data34[] = "\x22\x00\x00\x00\x05\x00\x1A\x00\x00\x00";
	unsigned char data35[] = "\x23\x00\x00\x00\x05\x00\x1B\x00\x04\x00";
	unsigned char data36[] = "\x24\x00\x00\x00\x05\x00\x1D\x00\x0C\x01";
	unsigned char data37[] = "\x25\x00\x00\x00\x05\x00\x1E\x00\x0D\x01";
	unsigned char data38[] = "\x26\x00\x00\x00\x05\x00\x1F\x00\x00\x00";
	unsigned char data39[] = "\x27\x00\x00\x00\x05\x00\x20\x00\x00\x00";
	unsigned char data40[] = "\x28\x00\x00\x00\x05\x00\x21\x00\x01\x00";
	unsigned char data41[] = "\x29\x00\x00\x00\x05\x00\x22\x00\x01\x00";
	unsigned char data42[] = "\x2A\x00\x00\x00\x05\x00\x23\x00\x01\x00";
	unsigned char data43[] = "\x2B\x00\x00\x00\x05\x00\x24\x00\x00\x00";
	unsigned char data44[] = "\x2C\x00\x00\x00\x05\x00\x2C\x00\x46\x00";
	unsigned char data45[] = "\x2D\x00\x00\x00\x05\x00\x2D\x00\x2A\x00";
	unsigned char data46[] = "\x2E\x00\x00\x00\x05\x00\x2F\x00\x0E\x01";
	unsigned char data47[] = "\x2F\x00\x00\x00\x05\x00\x30\x00\x0F\x01";
	unsigned char data48[] = "\x30\x00\x00\x00\x05\x00\x32\x00\x04\x00";
	unsigned char data49[] = "\x31\x00\x00\x00\x05\x00\x33\x00\x03\x00";
	unsigned char data50[] = "\x32\x00\x00\x00\x05\x00\x34\x00\x05\x00";
	unsigned char data51[] = "\x33\x00\x00\x00\x05\x00\x35\x00\x10\x00";
	unsigned char data52[] = "\x34\x00\x00\x00\x05\x00\x36\x00\x0A\x00";
	unsigned char data53[] = "\x35\x00\x00\x00\x05\x00\x37\x00\x18\x00";
	unsigned char data54[] = "\x36\x00\x00\x00\x05\x00\x38\x00\x1E\x00";
	unsigned char data55[] = "\x37\x00\x00\x00\x05\x00\x39\x00\xB2\x00";
	unsigned char data56[] = "\x38\x00\x00\x00\x05\x00\x3A\x00\x80\x00";
	unsigned char data57[] = "\x39\x00\x00\x00\x05\x00\x3B\x00\x00\x00";
	unsigned char data58[] = "\x3A\x00\x00\x00\x05\x00\x3E\x00\xF4\x01";
	unsigned char data59[] = "\x3B\x00\x00\x00\x05\x00\x3F\x00\x1A\x00";
	unsigned char data60[] = "\x3C\x00\x00\x00\x05\x00\x40\x00\xF8\x00";
	unsigned char data61[] = "\x3D\x00\x00\x00\x05\x00\x42\x00\x01\x00";
	unsigned char data62[] = "\x3E\x00\x00\x00\x05\x00\x43\x00\x04\x00";
	unsigned char data63[] = "\x3F\x00\x00\x00\x05\x00\x44\x00\x10\x00";
	unsigned char data64[] = "\x40\x00\x00\x00\x05\x00\x45\x00\x05\x00";
	unsigned char data65[] = "\x41\x00\x00\x00\x05\x00\x46\x00\xF5\x00";
	unsigned char data66[] = "\x42\x00\x00\x00\x05\x00\x47\x00\x0C\x00";
	unsigned char data67[] = "\x43\x00\x00\x00\x05\x00\x48\x00\x00\x00";
	unsigned char data68[] = "\x44\x00\x00\x00\x05\x00\x49\x00\x00\x00";
	unsigned char data69[] = "\x45\x00\x00\x00\x05\x00\x53\x00\x00\x00";
	unsigned char data70[] = "\x46\x00\x00\x00\x05\x00\x56\x00\xB4\x00";
	unsigned char data71[] = "\x47\x00\x00\x00\x05\x00\x57\x00\x96\x00";
	unsigned char data72[] = "\x48\x00\x00\x00\x05\x00\x58\x00\x8C\x00";
	unsigned char data73[] = "\x49\x00\x00\x00\x05\x00\x59\x00\x64\x00";
	unsigned char data74[] = "\x4A\x00\x00\x00\x05\x00\x5B\x00\x01\x00";
	unsigned char data75[] = "\x4B\x00\x00\x00\x05\x00\x5C\x00\x01\x00";
	unsigned char data76[] = "\x4C\x00\x00\x00\x05\x00\x5D\x00\x20\x00";
	unsigned char data77[] = "\x4D\x00\x00\x00\x05\x00\x5E\x00\x64\x00";
	unsigned char data78[] = "\x4E\x00\x00\x00\x05\x00\x5F\x00\xC8\x00";
	unsigned char data79[] = "\x4F\x00\x00\x00\x05\x00\x60\x00\xC8\x00";
	unsigned char data80[] = "\x50\x00\x00\x00\x05\x00\x62\x00\x00\x00";
	unsigned char data81[] = "\x51\x00\x00\x00\x05\x00\x64\x00\x1A\x01";
	unsigned char data82[] = "\x52\x00\x00\x00\x05\x00\x69\x00\x14\x00";
	unsigned char data83[] = "\x53\x00\x00\x00\x04\x00\x2A\x00";
	unsigned char data84[] = "\x54\x00\x00\x00\x04\x00\x3C\x00";
	unsigned char data85[] = "\x55\x00\x00\x00\x04\x00\x4A\x00";
	unsigned char data86[] = "\x56\x00\x00\x00\x04\x00\x41\x00";
	unsigned char data87[] = "\x57\x00\x00\x00\x02\x00\x00";
	unsigned char data88[] = "\x58\x00\x00\x00\x05\x00\x63\x00\x01\x00";
	unsigned char data89[] = "\x59\x00\x00\x00\x14\x00\x05\x00\xAB\x00\x00\x00\x00";
	unsigned char data90[] = "\x5A\x00\x00\x00\x05\x00\x64\x00\x18\x01";
	unsigned char data91[] = "\x5B\x00\x00\x00\x06\x00";
	unsigned char data92[] = "\x5C\x00\x00\x00\x05\x00\x46\x00\xF5\x00";
	unsigned char data93[] = "\x5D\x00\x00\x00\x05\x00\x55\x00\x08\x00";
	unsigned char data94[] = "\x5E\x00\x00\x00\x05\x00\x6D\x00\x32\x00";
	unsigned char data95[] = "\x5F\x00\x00\x00\x05\x00\x6E\x00\x03\x00";
	unsigned char data96[] = "\x60\x00\x00\x00\x04\x00\x52\x00";
	unsigned char data97[] = "\x61\x00\x00\x00\x05\x00\x52\x00\x20\x03";
	unsigned char data98[] = "\x62\x00\x00\x00\x03\x00\x01\x00\x00\x00\x00\x00\x00\x01";
	unsigned char data99[] = "\x63\x00\x00\x00\x05\x00\x52\x00\xB4\x1E";
	unsigned char data100[] = "\x64\x00\x00\x00\x12\x00\x2C\x50\xFF\x00\x02";
	unsigned char data101[] = "\x65\x00\x00\x00\x12\x00\x2E\x50\xFF\x00\x02";
	unsigned char data102[] = "\x66\x00\x00\x00\x13\x00\xF6\x05\x00\x00\x01\x00\x00\x00\x01";
	unsigned char data103[] = "\x67\x00\x00\x00\x12\x00\x3E\x50\xFF\x00\x01";
	unsigned char data104[] = "\x68\x00\x00\x00\x13\x00\x3E\x50\xFF\x00\x00\x00\x00\x00\x01";
	unsigned char data105[] = "\x69\x00\x00\x00\x12\x00\x02\x98\xFF\x00\x01";
	unsigned char data106[] = "\x6A\x00\x00\x00\x12\x00\x00\x98\xFF\x00\x01";
	unsigned char data107[] = "\x6B\x00\x00\x00\x12\x00\x06\x98\xFF\x00\x01";
	unsigned char data108[] = "\x6C\x00\x00\x00\x13\x00\x06\x98\xFF\x00\x00\x00\x00\x00\x01";
	unsigned char data109[] = "\x6D\x00\x00\x00\x03\x00\x64\x00\x00\x01\x00\x00\x00\x01";
	unsigned char data110[] = "\x6E\x00\x00\x00\x13\x00\xF6\x05\x00\x00\x00\x00\x00\x00\x01";
	unsigned char data111[] = "\x6F\x00\x00\x00\x12\x00\x3E\x50\xFF\x00\x01";
	unsigned char data112[] = "\x70\x00\x00\x00\x13\x00\x3E\x50\xFF\x00\x10\x00\x00\x00\x01";
	unsigned char data113[] = "\x71\x00\x00\x00\x12\x00\x02\x98\xFF\x00\x01";
	unsigned char data114[] = "\x72\x00\x00\x00\x12\x00\x00\x98\xFF\x00\x01";
	unsigned char data115[] = "\x73\x00\x00\x00\x12\x00\x06\x98\xFF\x00\x01";
	unsigned char data116[] = "\x74\x00\x00\x00\x03\x00\x64\x00\x00\x01\x00\x00\x00\x01";
	unsigned char data117[] = "\x75\x00\x00\x00\x12\x00\x38\x50\xFF\x00\x01";
	unsigned char data118[] = "\x76\x00\x00\x00\x12\x00\x0E\x50\xFF\x00\x02";
	unsigned char data119[] = "\x77\x00\x00\x00\x12\x00\x32\x50\xFF\x00\x01";
	unsigned char data120[] = "\x78\x00\x00\x00\x13\x00\x32\x50\xFF\x00\x12\x00\x00\x00\x01";
	unsigned char data121[] = "\x79\x00\x00\x00\x13\x00\x0E\x50\xFF\x00\x00\x40\x00\x00\x02";
	unsigned char data122[] = "\x7A\x00\x00\x00\x13\x00\x38\x50\xFF\x00\x0F\x00\x00\x00\x01";
	unsigned char data123[] = "\x7B\x00\x00\x00\x05\x00\x62\x00\x00\x00";
	unsigned char data124[] = "\x7C\x00\x00\x00\x05\x00\x77\x00\x00\x00";
	unsigned char data125[] = "\x7D\x00\x00\x00\x05\x00\x76\x00\x00\x00";
	unsigned char data126[] = "\x7E\x00\x00\x00\x05\x00\x78\x00\x00\x00";
	unsigned char data127[] = "\x7F\x00\x00\x00\x03\x00\x02\x00\x00\x01\x00\x00\x00\x01";
	unsigned char data128[] = "\x80\x00\x00\x00\x13\x00\xF6\x05\x00\x00\x01\x00\x00\x00\x01";
	unsigned char data129[] = "\x81\x00\x00\x00\x12\x00\x3E\x50\xFF\x00\x01";
	unsigned char data130[] = "\x82\x00\x00\x00\x13\x00\x3E\x50\xFF\x00\x00\x00\x00\x00\x01";
	unsigned char data131[] = "\x83\x00\x00\x00\x12\x00\x02\x98\xFF\x00\x01";
	unsigned char data132[] = "\x84\x00\x00\x00\x12\x00\x00\x98\xFF\x00\x01";
	unsigned char data133[] = "\x85\x00\x00\x00\x12\x00\x06\x98\xFF\x00\x01";
	unsigned char data134[] = "\x86\x00\x00\x00\x13\x00\x38\x50\xFF\x00\x0E\x00\x00\x00\x01";
	unsigned char data135[] = "\x87\x00\x00\x00\x03\x00\x0A\x00\x00\x01\x00\x00\x00\x01";
	unsigned char data136[] = "\x88\x00\x00\x00\x13\x00\x38\x50\xFF\x00\x0D\x00\x00\x00\x01";
	unsigned char data137[] = "\x89\x00\x00\x00\x03\x00\x0A\x00\x00\x01\x00\x00\x00\x01";
	unsigned char data138[] = "\x8A\x00\x00\x00\x13\x00\x38\x50\xFF\x00\x0C\x00\x00\x00\x01";
	unsigned char data139[] = "\x8B\x00\x00\x00\x03\x00\x0A\x00\x00\x01\x00\x00\x00\x01";
	unsigned char data140[] = "\x8C\x00\x00\x00\x13\x00\x38\x50\xFF\x00\x0B\x00\x00\x00\x01";
	unsigned char data141[] = "\x8D\x00\x00\x00\x03\x00\x0A\x00\x00\x01\x00\x00\x00\x01";
	unsigned char data142[] = "\x8E\x00\x00\x00\x13\x00\x38\x50\xFF\x00\x0A\x00\x00\x00\x01";
	unsigned char data143[] = "\x8F\x00\x00\x00\x03\x00\x0A\x00\x00\x01\x00\x00\x00\x01";
	unsigned char data144[] = "\x90\x00\x00\x00\x13\x00\x38\x50\xFF\x00\x09\x00\x00\x00\x01";
	unsigned char data145[] = "\x91\x00\x00\x00\x03\x00\x0A\x00\x00\x01\x00\x00\x00\x01";
	unsigned char data146[] = "\x92\x00\x00\x00\x13\x00\x38\x50\xFF\x00\x08\x00\x00\x00\x01";
	unsigned char data147[] = "\x93\x00\x00\x00\x03\x00\x0A\x00\x00\x01\x00\x00\x00\x01";
	unsigned char data148[] = "\x94\x00\x00\x00\x13\x00\x38\x50\xFF\x00\x07\x00\x00\x00\x01";
	unsigned char data149[] = "\x95\x00\x00\x00\x03\x00\x0A\x00\x00\x01\x00\x00\x00\x01";
	unsigned char data150[] = "\x96\x00\x00\x00\x13\x00\xF6\x05\x00\x00\x00\x00\x00\x00\x01";
	unsigned char data151[] = "\x97\x00\x00\x00\x12\x00\x3E\x50\xFF\x00\x01";
	unsigned char data152[] = "\x98\x00\x00\x00\x13\x00\x3E\x50\xFF\x00\x10\x00\x00\x00\x01";
	unsigned char data153[] = "\x99\x00\x00\x00\x12\x00\x02\x98\xFF\x00\x01";
	unsigned char data154[] = "\x9A\x00\x00\x00\x12\x00\x00\x98\xFF\x00\x01";
	unsigned char data155[] = "\x9B\x00\x00\x00\x12\x00\x06\x98\xFF\x00\x01";
	unsigned char data156[] = "\x9C\x00\x00\x00\x13\x00\x06\x98\xFF\x00\x00\x00\x00\x00\x01";
	unsigned char data157[] = "\x9D\x00\x00\x00\x03\x00\x0A\x00\x00\x01\x00\x00\x00\x01";
	unsigned char data158[] = "\x9E\x00\x00\x00\x05\x00\x77\x00\x07\x00";
	unsigned char data159[] = "\x9F\x00\x00\x00\x05\x00\x76\x00\x12\x00";
	unsigned char data160[] = "\xA0\x00\x00\x00\x05\x00\x78\x00\xA0\x21";
	unsigned char data161[] = "\xA1\x00\x00\x00\x13\x00\x38\x50\xFF\x00\x14\x00\x00\x00\x01";
	unsigned char data162[] = "\xA2\x00\x00\x00\x13\x00\x0E\x50\xFF\x00\xB4\x21\x00\x00\x02";
	unsigned char data163[] = "\xA3\x00\x00\x00\x13\x00\x32\x50\xFF\x00\x31\x00\x00\x00\x01";
	unsigned char data164[] = "\xA4\x00\x00\x00\x05\x00\x62\x00\x32\x00";
	unsigned char data165[] = "\xA5\x00\x00\x00\x0E\x00";
	unsigned char data166[] = "\xA6\x00\x00\x00\x05\x00\x62\x00\x32\x00";
	unsigned char data167[] = "\xA7\x00\x00\x00\x04\x00\x14\x00";
	unsigned char data168[] = "\xA8\x00\x00\x00\x04\x00\x11\x00";
	unsigned char data169[] = "\xA9\x00\x00\x00\x05\x00\x62\x00\x32\x00";
	unsigned char data170[] = "\xAA\x00\x00\x00\x03\x00\x14\x00\x00\x01\x00\x00\x00\x01";
	unsigned char data171[] = "\xAB\x00\x00\x00\x04\x00\x14\x00";
	unsigned char data172[] = "\xAC\x00\x00\x00\x04\x00\x14\x00";
	unsigned char data173[] = "\xAD\x00\x00\x00\x0E\x00";
	unsigned char data174[] = "\xAE\x00\x00\x00\x04\x00\x11\x00";
	unsigned char data175[] = "\xAF\x00\x00\x00\x05\x00\x62\x00\x32\x00";
	unsigned char data176[] = "\xB0\x00\x00\x00\x03\x00\x88\x13\x01\x00\x00\x00\x01\x01";
	unsigned char data177[] = "\xB1\x00\x00\x00\x16\x00";
	unsigned char data178[] = "\xB2\x00\x00\x00\x16\x00";
	unsigned char data179[] = "\xB3\x00\x00\x00\x16\x00";
	unsigned char data180[] = "\xB4\x00\x00\x00\x16\x00";
	unsigned char data181[] = "\xB5\x00\x00\x00\x16\x00";
	unsigned char data182[] = "\xB6\x00\x00\x00\x16\x00";
	unsigned char data183[] = "\xB7\x00\x00\x00\x16\x00";
	unsigned char data184[] = "\xB8\x00\x00\x00\x16\x00";
	unsigned char data185[] = "\xB9\x00\x00\x00\x16\x00";
	unsigned char data186[] = "\xBA\x00\x00\x00\x16\x00";
	unsigned char data187[] = "\xBB\x00\x00\x00\x16\x00";
        int r = 0;
	validity_swap_messages(dev, data1, (int) sizeof(data1) - 1); 
	validity_swap_messages(dev, data2, (int) sizeof(data2) - 1); 
	validity_swap_messages(dev, data3, (int) sizeof(data3) - 1); 
	validity_swap_messages(dev, data4, (int) sizeof(data4) - 1); 
	validity_swap_messages(dev, data5, (int) sizeof(data5) - 1); 
	validity_swap_messages(dev, data6, (int) sizeof(data6) - 1); 
	validity_swap_messages(dev, data7, (int) sizeof(data7) - 1); 
	validity_swap_messages(dev, data8, (int) sizeof(data8) - 1); 
	validity_swap_messages(dev, data9, (int) sizeof(data9) - 1); 
	validity_swap_messages(dev, data10, (int) sizeof(data10) - 1); 
	validity_swap_messages(dev, data11, (int) sizeof(data11) - 1); 
	validity_swap_messages(dev, data12, (int) sizeof(data12) - 1); 
	validity_swap_messages(dev, data13, (int) sizeof(data13) - 1); 
	validity_swap_messages(dev, data14, (int) sizeof(data14) - 1); 
	validity_swap_messages(dev, data15, (int) sizeof(data15) - 1); 
	r = validity_receive_long_data(dev);
	r = validity_receive_long_data(dev);
	validity_swap_messages(dev, data16, (int) sizeof(data16) - 1); 
	validity_swap_messages(dev, data17, (int) sizeof(data17) - 1); 
	validity_swap_messages(dev, data18, (int) sizeof(data18) - 1); 
	validity_swap_messages(dev, data19, (int) sizeof(data19) - 1); 
	validity_swap_messages(dev, data20, (int) sizeof(data20) - 1); 
	validity_swap_messages(dev, data21, (int) sizeof(data21) - 1); 
	validity_swap_messages(dev, data22, (int) sizeof(data22) - 1); 
	validity_swap_messages(dev, data23, (int) sizeof(data23) - 1); 
	validity_swap_messages(dev, data24, (int) sizeof(data24) - 1); 
	validity_swap_messages(dev, data25, (int) sizeof(data25) - 1); 
	validity_swap_messages(dev, data26, (int) sizeof(data26) - 1); 
	validity_swap_messages(dev, data27, (int) sizeof(data27) - 1); 
	validity_swap_messages(dev, data28, (int) sizeof(data28) - 1); 
	validity_swap_messages(dev, data29, (int) sizeof(data29) - 1); 
	validity_swap_messages(dev, data30, (int) sizeof(data30) - 1); 
	validity_swap_messages(dev, data31, (int) sizeof(data31) - 1); 
	validity_swap_messages(dev, data32, (int) sizeof(data32) - 1); 
	validity_swap_messages(dev, data33, (int) sizeof(data33) - 1); 
	validity_swap_messages(dev, data34, (int) sizeof(data34) - 1); 
	validity_swap_messages(dev, data35, (int) sizeof(data35) - 1); 
	validity_swap_messages(dev, data36, (int) sizeof(data36) - 1); 
	validity_swap_messages(dev, data37, (int) sizeof(data37) - 1); 
	validity_swap_messages(dev, data38, (int) sizeof(data38) - 1); 
	validity_swap_messages(dev, data39, (int) sizeof(data39) - 1); 
	validity_swap_messages(dev, data40, (int) sizeof(data40) - 1); 
	validity_swap_messages(dev, data41, (int) sizeof(data41) - 1); 
	validity_swap_messages(dev, data42, (int) sizeof(data42) - 1); 
	validity_swap_messages(dev, data43, (int) sizeof(data43) - 1); 
	validity_swap_messages(dev, data44, (int) sizeof(data44) - 1); 
	validity_swap_messages(dev, data45, (int) sizeof(data45) - 1); 
	validity_swap_messages(dev, data46, (int) sizeof(data46) - 1); 
	validity_swap_messages(dev, data47, (int) sizeof(data47) - 1); 
	validity_swap_messages(dev, data48, (int) sizeof(data48) - 1); 
	validity_swap_messages(dev, data49, (int) sizeof(data49) - 1); 
	validity_swap_messages(dev, data50, (int) sizeof(data50) - 1); 
	validity_swap_messages(dev, data51, (int) sizeof(data51) - 1); 
	validity_swap_messages(dev, data52, (int) sizeof(data52) - 1); 
	validity_swap_messages(dev, data53, (int) sizeof(data53) - 1); 
	validity_swap_messages(dev, data54, (int) sizeof(data54) - 1); 
	validity_swap_messages(dev, data55, (int) sizeof(data55) - 1); 
	validity_swap_messages(dev, data56, (int) sizeof(data56) - 1); 
	validity_swap_messages(dev, data57, (int) sizeof(data57) - 1); 
	validity_swap_messages(dev, data58, (int) sizeof(data58) - 1); 
	validity_swap_messages(dev, data59, (int) sizeof(data59) - 1); 
	validity_swap_messages(dev, data60, (int) sizeof(data60) - 1); 
	validity_swap_messages(dev, data61, (int) sizeof(data61) - 1); 
	validity_swap_messages(dev, data62, (int) sizeof(data62) - 1); 
	validity_swap_messages(dev, data63, (int) sizeof(data63) - 1); 
	validity_swap_messages(dev, data64, (int) sizeof(data64) - 1); 
	validity_swap_messages(dev, data65, (int) sizeof(data65) - 1); 
	validity_swap_messages(dev, data66, (int) sizeof(data66) - 1); 
	validity_swap_messages(dev, data67, (int) sizeof(data67) - 1); 
	validity_swap_messages(dev, data68, (int) sizeof(data68) - 1); 
	validity_swap_messages(dev, data69, (int) sizeof(data69) - 1); 
	validity_swap_messages(dev, data70, (int) sizeof(data70) - 1); 
	validity_swap_messages(dev, data71, (int) sizeof(data71) - 1); 
	validity_swap_messages(dev, data72, (int) sizeof(data72) - 1); 
	validity_swap_messages(dev, data73, (int) sizeof(data73) - 1); 
	validity_swap_messages(dev, data74, (int) sizeof(data74) - 1); 
	validity_swap_messages(dev, data75, (int) sizeof(data75) - 1); 
	validity_swap_messages(dev, data76, (int) sizeof(data76) - 1); 
	validity_swap_messages(dev, data77, (int) sizeof(data77) - 1); 
	validity_swap_messages(dev, data78, (int) sizeof(data78) - 1); 
	validity_swap_messages(dev, data79, (int) sizeof(data79) - 1); 
	validity_swap_messages(dev, data80, (int) sizeof(data80) - 1); 
	validity_swap_messages(dev, data81, (int) sizeof(data81) - 1); 
	validity_swap_messages(dev, data82, (int) sizeof(data82) - 1); 
	validity_swap_messages(dev, data83, (int) sizeof(data83) - 1); 
	validity_swap_messages(dev, data84, (int) sizeof(data84) - 1); 
	validity_swap_messages(dev, data85, (int) sizeof(data85) - 1); 
	validity_swap_messages(dev, data86, (int) sizeof(data86) - 1); 
	validity_swap_messages(dev, data87, (int) sizeof(data87) - 1); 
	validity_swap_messages(dev, data88, (int) sizeof(data88) - 1); 
	validity_swap_messages(dev, data89, (int) sizeof(data89) - 1); 
	validity_swap_messages(dev, data90, (int) sizeof(data90) - 1); 
	validity_swap_messages(dev, data91, (int) sizeof(data91) - 1); 
	validity_swap_messages(dev, data92, (int) sizeof(data92) - 1); 
	validity_swap_messages(dev, data93, (int) sizeof(data93) - 1); 
	validity_swap_messages(dev, data94, (int) sizeof(data94) - 1); 
	validity_swap_messages(dev, data95, (int) sizeof(data95) - 1); 
	validity_swap_messages(dev, data96, (int) sizeof(data96) - 1); 
	validity_swap_messages(dev, data97, (int) sizeof(data97) - 1); 
	validity_swap_messages(dev, data98, (int) sizeof(data98) - 1); 
	r = validity_receive_long_data(dev);
	validity_swap_messages(dev, data99, (int) sizeof(data99) - 1); 
	validity_swap_messages(dev, data100, (int) sizeof(data100) - 1); 
	validity_swap_messages(dev, data101, (int) sizeof(data101) - 1); 
	validity_swap_messages(dev, data102, (int) sizeof(data102) - 1); 
	validity_swap_messages(dev, data103, (int) sizeof(data103) - 1); 
	validity_swap_messages(dev, data104, (int) sizeof(data104) - 1); 
	validity_swap_messages(dev, data105, (int) sizeof(data105) - 1); 
	validity_swap_messages(dev, data106, (int) sizeof(data106) - 1); 
	validity_swap_messages(dev, data107, (int) sizeof(data107) - 1); 
	validity_swap_messages(dev, data108, (int) sizeof(data108) - 1); 
	validity_swap_messages(dev, data109, (int) sizeof(data109) - 1); 
	r = validity_receive_long_data(dev);
	validity_swap_messages(dev, data110, (int) sizeof(data110) - 1); 
	validity_swap_messages(dev, data111, (int) sizeof(data111) - 1); 
	validity_swap_messages(dev, data112, (int) sizeof(data112) - 1); 
	validity_swap_messages(dev, data113, (int) sizeof(data113) - 1); 
	validity_swap_messages(dev, data114, (int) sizeof(data114) - 1); 
	validity_swap_messages(dev, data115, (int) sizeof(data115) - 1); 
	validity_swap_messages(dev, data116, (int) sizeof(data116) - 1); 
	r = validity_receive_long_data(dev);
	validity_swap_messages(dev, data117, (int) sizeof(data117) - 1); 
	validity_swap_messages(dev, data118, (int) sizeof(data118) - 1); 
	validity_swap_messages(dev, data119, (int) sizeof(data119) - 1); 
	validity_swap_messages(dev, data120, (int) sizeof(data120) - 1); 
	validity_swap_messages(dev, data121, (int) sizeof(data121) - 1); 
	validity_swap_messages(dev, data122, (int) sizeof(data122) - 1); 
	validity_swap_messages(dev, data123, (int) sizeof(data123) - 1); 
	validity_swap_messages(dev, data124, (int) sizeof(data124) - 1); 
	validity_swap_messages(dev, data125, (int) sizeof(data125) - 1); 
	validity_swap_messages(dev, data126, (int) sizeof(data126) - 1); 
	validity_swap_messages(dev, data127, (int) sizeof(data127) - 1); 
	r = validity_receive_long_data(dev);
	validity_swap_messages(dev, data128, (int) sizeof(data128) - 1); 
	validity_swap_messages(dev, data129, (int) sizeof(data129) - 1); 
	validity_swap_messages(dev, data130, (int) sizeof(data130) - 1); 
	validity_swap_messages(dev, data131, (int) sizeof(data131) - 1); 
	validity_swap_messages(dev, data132, (int) sizeof(data132) - 1); 
	validity_swap_messages(dev, data133, (int) sizeof(data133) - 1); 
	validity_swap_messages(dev, data134, (int) sizeof(data134) - 1); 
	validity_swap_messages(dev, data135, (int) sizeof(data135) - 1); 
	r = validity_receive_long_data(dev);
	validity_swap_messages(dev, data136, (int) sizeof(data136) - 1); 
	validity_swap_messages(dev, data137, (int) sizeof(data137) - 1); 
	r = validity_receive_long_data(dev);
	validity_swap_messages(dev, data138, (int) sizeof(data138) - 1); 
	validity_swap_messages(dev, data139, (int) sizeof(data139) - 1); 
	r = validity_receive_long_data(dev);
	validity_swap_messages(dev, data140, (int) sizeof(data140) - 1); 
	validity_swap_messages(dev, data141, (int) sizeof(data141) - 1); 
	r = validity_receive_long_data(dev);
	validity_swap_messages(dev, data142, (int) sizeof(data142) - 1); 
	validity_swap_messages(dev, data143, (int) sizeof(data143) - 1); 
	r = validity_receive_long_data(dev);
	validity_swap_messages(dev, data144, (int) sizeof(data144) - 1); 
	validity_swap_messages(dev, data145, (int) sizeof(data145) - 1); 
	r = validity_receive_long_data(dev);
	validity_swap_messages(dev, data146, (int) sizeof(data146) - 1); 
	validity_swap_messages(dev, data147, (int) sizeof(data147) - 1); 
	r = validity_receive_long_data(dev);
	validity_swap_messages(dev, data148, (int) sizeof(data148) - 1); 
	validity_swap_messages(dev, data149, (int) sizeof(data149) - 1); 
	r = validity_receive_long_data(dev);
	validity_swap_messages(dev, data150, (int) sizeof(data150) - 1); 
	validity_swap_messages(dev, data151, (int) sizeof(data151) - 1); 
	validity_swap_messages(dev, data152, (int) sizeof(data152) - 1); 
	validity_swap_messages(dev, data153, (int) sizeof(data153) - 1); 
	validity_swap_messages(dev, data154, (int) sizeof(data154) - 1); 
	validity_swap_messages(dev, data155, (int) sizeof(data155) - 1); 
	validity_swap_messages(dev, data156, (int) sizeof(data156) - 1); 
	validity_swap_messages(dev, data157, (int) sizeof(data157) - 1); 
	r = validity_receive_long_data(dev);
	validity_swap_messages(dev, data158, (int) sizeof(data158) - 1); 
	validity_swap_messages(dev, data159, (int) sizeof(data159) - 1); 
	validity_swap_messages(dev, data160, (int) sizeof(data160) - 1); 
	validity_swap_messages(dev, data161, (int) sizeof(data161) - 1); 
	validity_swap_messages(dev, data162, (int) sizeof(data162) - 1); 
	validity_swap_messages(dev, data163, (int) sizeof(data163) - 1); 
	validity_swap_messages(dev, data164, (int) sizeof(data164) - 1); 
	validity_swap_messages(dev, data165, (int) sizeof(data165) - 1); 
	r = validity_receive_long_data(dev);
	r = validity_receive_long_data(dev);
	validity_swap_messages(dev, data166, (int) sizeof(data166) - 1); 
	validity_swap_messages(dev, data167, (int) sizeof(data167) - 1); 
	validity_swap_messages(dev, data168, (int) sizeof(data168) - 1); 
	validity_swap_messages(dev, data169, (int) sizeof(data169) - 1); 
	validity_swap_messages(dev, data170, (int) sizeof(data170) - 1); 
	r = validity_receive_long_data(dev);
	validity_swap_messages(dev, data171, (int) sizeof(data171) - 1); 
	validity_swap_messages(dev, data172, (int) sizeof(data172) - 1); 
	validity_swap_messages(dev, data173, (int) sizeof(data173) - 1); 
	r = validity_receive_long_data(dev);
	r = validity_receive_long_data(dev);
	validity_swap_messages(dev, data174, (int) sizeof(data174) - 1); 
	validity_swap_messages(dev, data175, (int) sizeof(data175) - 1); 
	validity_swap_messages(dev, data176, (int) sizeof(data176) - 1); 

	int i = 0;
	for (i; i < 50; i++)
		validity_swap_messages(dev, data177, (int) sizeof(data177) - 1); 
		usleep(750000);
	return 0;
}

static int validity_cycle2(struct vfs_dev *dev){
	unsigned char data00 [] = "\x01\x00\x00\x00\x12\x00\xE8\x1F\x00\x00\x04";
	unsigned char data01 [] = "\x02\x00\x00\x00\x12\x00\xEC\x1F\x00\x00\x04";
	unsigned char data02 [] = "\x03\x00\x00\x00\x12\x00\xF0\x1F\x00\x00\x04";
	unsigned char data03 [] = "\x04\x00\x00\x00\x12\x00\xF4\x1F\x00\x00\x04";
	unsigned char data04 [] = "\x05\x00\x00\x00\x12\x00\xF8\x1F\x00\x00\x04";
	unsigned char data05 [] = "\x06\x00\x00\x00\x12\x00\xFC\x1F\x00\x00\x04";
	unsigned char data[] = "\xF5\x00\x00\x00\x16\x00"; 
	
	validity_swap_messages(dev, data00, (int) sizeof(data00) - 1);
	validity_swap_messages(dev, data01, (int) sizeof(data01) - 1);
	validity_swap_messages(dev, data02, (int) sizeof(data02) - 1);
	validity_swap_messages(dev, data03, (int) sizeof(data03) - 1);
	validity_swap_messages(dev, data04, (int) sizeof(data04) - 1);
	validity_swap_messages(dev, data05, (int) sizeof(data05) - 1);

	int r = validity_swap_messages(dev, data, (int) sizeof(data) - 1);
	return r;
}

static int validity_cycle1(struct vfs_dev *dev){
	unsigned char data00 [] = "\x01\x00\x00\x00\x12\x00\xE8\x1F\x00\x00\x04";
	unsigned char data01 [] = "\x02\x00\x00\x00\x12\x00\xEC\x1F\x00\x00\x04";
	unsigned char data02 [] = "\x03\x00\x00\x00\x12\x00\xF0\x1F\x00\x00\x04";
	unsigned char data03 [] = "\x04\x00\x00\x00\x12\x00\xF4\x1F\x00\x00\x04";
	unsigned char data04 [] = "\x05\x00\x00\x00\x12\x00\xF8\x1F\x00\x00\x04";
	unsigned char data05 [] = "\x06\x00\x00\x00\x12\x00\xFC\x1F\x00\x00\x04";
	unsigned char data06 [] = "\x07\x00\x00\x00\x04\x00\x2E\x00";
	unsigned char data07 [] = "\x08\x00\x00\x00\x02\x00\x00";
	unsigned char data08 [] = "\x09\x00\x00\x00\x04\x00\x28\x00";
	unsigned char data09 [] = "\x0A\x00\x00\x00\x04\x00\x14\x00";
	unsigned char data10 [] = "\x0B\x00\x00\x00\x04\x00\x11\x00";
	unsigned char data11 [] = "\x0C\x00\x00\x00\x04\x00\x54\x00";
	unsigned char data12 [] = "\x0D\x00\x00\x00\x04\x00\x01\x00";
	unsigned char data13 [] = "\x0E\x00\x00\x00\x04\x00\x14\x00";
	unsigned char data14 [] = "\x0F\x00\x00\x00\x0E\x00";
	unsigned char data15 [] = "\x10\x00\x00\x00\x03\x00\x01\x00\x00\x01\x00\x00\x00\x01";
	unsigned char data16 [] = "\x11\x00\x00\x00\x0E\x00";
	unsigned char data17 [] = "\x12\x00\x00\x00\x05\x00\x04\x00\x00\x00";
	unsigned char data18 [] = "\x13\x00\x00\x00\x05\x00\x05\x00\x00\x00";
	unsigned char data19 [] = "\x14\x00\x00\x00\x05\x00\x06\x00\x00\x00";
	unsigned char data20 [] = "\x15\x00\x00\x00\x05\x00\x07\x00\x00\x00";
	unsigned char data21 [] = "\x16\x00\x00\x00\x05\x00\x0A\x00\x02\x00";
	unsigned char data22 [] = "\x17\x00\x00\x00\x05\x00\x0B\x00\x0B\x01";
	unsigned char data23 [] = "\x18\x00\x00\x00\x05\x00\x0C\x00\x0C\x01";
	unsigned char data24 [] = "\x19\x00\x00\x00\x05\x00\x0D\x00\x0D\x01";
	unsigned char data25 [] = "\x1A\x00\x00\x00\x05\x00\x0E\x00\x01\x00";
	unsigned char data26 [] = "\x1B\x00\x00\x00\x05\x00\x10\x00\x00\x00";
	unsigned char data27 [] = "\x1C\x00\x00\x00\x05\x00\x12\x00\x40\x00";
	unsigned char data28 [] = "\x1D\x00\x00\x00\x05\x00\x15\x00\xB0\x04";
	unsigned char data29 [] = "\x1E\x00\x00\x00\x05\x00\x16\x00\x1B\x00";
	unsigned char data30 [] = "\x1F\x00\x00\x00\x05\x00\x17\x00\x01\x00";
	unsigned char data31 [] = "\x20\x00\x00\x00\x05\x00\x18\x00\x03\x00";


	unsigned char data32 [] = "\x00\x65\x00\x00\x04\x00\x14\x00";
	unsigned char data33 [] = "\x01\x65\x00\x00\x04\x00\x14\x00";
	unsigned char data34 [] = "\x02\x65\x00\x00\x0E\x00";
	unsigned char data35 [] = "\x03\x65\x00\x00\x04\x00\x11\x00";
	unsigned char data36 [] = "\x04\x65\x00\x00\x05\x00\x62\x00\x32\x00";
	unsigned char data37 [] = "\x05\x65\x00\x00\x03\x00\x88\x13\x01\x00\x00\x00\x01\x01";

	unsigned char data4[] = "\xF5\x00\x00\x00\x16\x00"; 

	int r = validity_swap_messages(dev, data00, (int) sizeof(data00) - 1);
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(dev, data01, (int) sizeof(data01) - 1);
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(dev, data02, (int) sizeof(data02) - 1);
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(dev, data03, (int) sizeof(data03) - 1);
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(dev, data04, (int) sizeof(data04) - 1);
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(dev, data05, (int) sizeof(data05) - 1);
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(dev, data06, (int) sizeof(data06) - 1);
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(dev, data07, (int) sizeof(data07) - 1);
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(dev, data08, (int) sizeof(data08) - 1);
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(dev, data09, (int) sizeof(data09) - 1);
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(dev, data10, (int) sizeof(data10) - 1);
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(dev, data11, (int) sizeof(data11) - 1);
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(dev, data12, (int) sizeof(data12) - 1);
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(dev, data13, (int) sizeof(data13) - 1);
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(dev, data14, (int) sizeof(data14) - 1);
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(dev, data15, (int) sizeof(data15) - 1);
	if (r != 0)                                               
		return r;                                         

	r = validity_receive_long_data(dev);
	r = validity_receive_long_data(dev);

	r = validity_swap_messages(dev, data16, (int) sizeof(data16) - 1);
	if (r != 0)                                               
		return r;                                         

	r = validity_receive_long_data(dev);
	
	r = validity_swap_messages(dev, data17, (int) sizeof(data17) - 1);
	if (r != 0)                                               
		return r;                                         

	r = validity_receive_long_data(dev);
	r = validity_receive_long_data(dev);

	r = validity_swap_messages(dev, data18, (int) sizeof(data18) - 1);
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(dev, data19, (int) sizeof(data19) - 1);
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(dev, data20, (int) sizeof(data20) - 1);
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(dev, data21, (int) sizeof(data21) - 1);
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(dev, data22, (int) sizeof(data22) - 1);
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(dev, data23, (int) sizeof(data23) - 1);
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(dev, data24, (int) sizeof(data24) - 1);
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(dev, data25, (int) sizeof(data25) - 1);
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(dev, data26, (int) sizeof(data26) - 1);
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(dev, data27, (int) sizeof(data27) - 1);
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(dev, data28, (int) sizeof(data28) - 1);
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(dev, data29, (int) sizeof(data29) - 1);
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(dev, data30, (int) sizeof(data30) - 1);
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(dev, data31, (int) sizeof(data31) - 1);
	if (r != 0)                                               
		return r;                                         

	r = validity_swap_messages(dev, data32, (int) sizeof(data32) - 1);
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(dev, data33, (int) sizeof(data33) - 1);
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(dev, data34, (int) sizeof(data34) - 1);
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(dev, data35, (int) sizeof(data35) - 1);
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(dev, data36, (int) sizeof(data36) - 1);
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(dev, data37, (int) sizeof(data37) - 1);
	if (r != 0)                                               
		return r;                                         

	int i = 0;
	for (i; i < 80; i++){
		r = validity_swap_messages(dev, data4, (int) sizeof(data4) - 1);
		if (r != 0)
			return r;
	}	
/*	
	r = validity_swap_messages(dev, data32, (int) sizeof(data32- 1));
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(dev, data33, (int) sizeof(data33- 1));
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(dev, data34, (int) sizeof(data34- 1));
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(dev, data35, (int) sizeof(data35- 1));
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(dev, data36, (int) sizeof(data36- 1));
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(dev, data37, (int) sizeof(data37- 1));
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(dev, data38, (int) sizeof(data38- 1));
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(dev, data39, (int) sizeof(data39- 1));
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(dev, data40, (int) sizeof(data40- 1));
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(dev, data41, (int) sizeof(data41- 1));
	if (r != 0)
		return r;
*/

	return 0;
}

static int validity_cycle0(struct vfs_dev *dev){
	unsigned char data1[] = "\x01\x00\x00\x00\x12\x00\xE8\x1F\x00\x00\x04";
	unsigned char data2[] = "\x02\x00\x00\x00\x12\x00\xEC\x1F\x00\x00\x04";
	unsigned char data3[] = "\x03\x00\x00\x00\x12\x00\xF0\x1F\x00\x00\x04";
	unsigned char data4[] = "\x04\x00\x00\x00\x12\x00\xF4\x1F\x00\x00\x04";
	unsigned char data5[] = "\x05\x00\x00\x00\x12\x00\xF8\x1F\x00\x00\x04";
	unsigned char data6[] = "\x06\x00\x00\x00\x12\x00\xFC\x1F\x00\x00\x04";


	validity_swap_messages(dev, data1, (int) sizeof(data1) - 1); 
	validity_swap_messages(dev, data2, (int) sizeof(data2) - 1); 
	validity_swap_messages(dev, data3, (int) sizeof(data3) - 1); 
	validity_swap_messages(dev, data4, (int) sizeof(data4) - 1); 
	validity_swap_messages(dev, data5, (int) sizeof(data5) - 1); 
	validity_swap_messages(dev, data6, (int) sizeof(data6) - 1); 


	unsigned char data36 [] = "\x04\x65\x00\x00\x05\x00\x62\x00\x32\x00";
	unsigned char data171[] = "\xAB\x00\x00\x00\x03\x00\x14\x00\x00\x01\x00\x00\x00\x01";
	unsigned char data44[] = "\xF5\x00\x00\x00\x16\x00"; 

	validity_swap_messages(dev, data36, (int) sizeof(data36) - 1);
//	validity_swap_messages(dev, data171, (int) sizeof(data171) - 1);
//	validity_receive_long_data(dev)	;

	int i = 0;

	for (i; i < 10; i++){
		validity_swap_messages(dev, data44, (int) sizeof(data44) - 1);
	}

	return 0; 
}
	
static int validity_cycle(struct vfs_dev *dev){
	unsigned char data[] = "\x00\x00\x00\x00\x12\x00\xE8\x1F\x00\x00\x04";//{packet_num_l, packet_num_h, 0x00, 0x00, 0x12, 0x00, 0xE8, 0x1F, 0x00, 0x00, 0x04};
	unsigned char data1[] = "\x02\x00\x00\x00\x12\x00\xEC\x1F\x00\x00\x04";
	unsigned char data2[] = "\x10\x00\x00\x00\x03\x00\x01\x00\x00\x01\x00\x00\x00\x01";
	unsigned char data3[] = "\xA4\x00\x00\x00\x0E\x00";
	unsigned char data4[] = "\xF5\x00\x00\x00\x16\x00"; 
	unsigned char data5[] = "\xB1\x00\x00\x00\x05\x00\x62\x00\x32\x00"; 
	unsigned char data6[] = "\xB2\x00\x00\x00\x03\x00\x88\x13\x01\x00\x00\x00\x01\x01"; 
	unsigned char data7[] = "\xB0\x00\x00\x00\x04\x00\x11\x00";

	int r = validity_swap_messages(dev, data, (int) sizeof(data) - 1);
	if (r != 0)
		return r;
	r = validity_swap_messages(dev, data1, (int) sizeof(data1) - 1);
	if (r != 0)
		return r;
	r = validity_swap_messages(dev, data2, (int) sizeof(data2) - 1);
	if (r != 0)
		return r;
	r = validity_receive_long_data(dev);

	r = validity_swap_messages(dev, data3, (int) sizeof(data3) - 1);
	if (r != 0)
		return r;

	r = validity_swap_messages(dev, data7, (int) sizeof(data7) - 1);
	if (r != 0)
		return r;

	r = validity_swap_messages(dev, data5, (int) sizeof(data5) - 1);
	if (r != 0)
		return r;

	r = validity_swap_messages(dev, data6, (int) sizeof(data6) - 1);
	if (r != 0)
		return r;

	int i = 0;
	for (i; i < 20; i++){
		r = validity_swap_messages(dev, data4, (int) sizeof(data4) - 1);
		if (r != 0)
			return r;
	}	

	return 0;	
}

/** Main function */
int main(void)
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
	r = validity_cycle4(dev);
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
