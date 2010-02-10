#include <errno.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <libusb-1.0/libusb.h>

/** Constants */
const int VALIDITY_DEFAULT_WAIT_TIMEOUT = 5000;
const unsigned char VALIDITY_SEND_ENDPOINT = 0x01;
const unsigned char VALIDITY_RECEIVE_ENDPOINT = 0x81;
const unsigned char VALIDITY_RECEIVE_ENDPOINT_LONG = 0x82; 

/** Variables */
static struct libusb_device_handle *devh = NULL;
static struct libusb_context *ctx = NULL;
static int packet_num_l = 1;
static int packet_num_h = 0;


/** Functions */

/** Searching our device */
static int validity_find_device(void)
{
	devh = libusb_open_device_with_vid_pid(NULL, 0x138a, 0x0001);
	return devh ? 0 : -EIO;
}

/** Configuring device */
static int validity_configure_device(void){
	int r = libusb_control_transfer(devh, LIBUSB_REQUEST_TYPE_STANDARD, LIBUSB_REQUEST_SET_FEATURE, 1, 0, NULL, 0, VALIDITY_DEFAULT_WAIT_TIMEOUT); 
	return r;
}

static void validity_numerate_packet(unsigned char *data){
	unsigned char *pNum = data;
	*pNum = packet_num_l;
	pNum = pNum + 1;
	*pNum = packet_num_h;
	packet_num_l++;
	if (packet_num_l == 0x100){
		packet_num_l = 0;
		packet_num_h++;
	}
}

static void validity_print_packet(unsigned char *data, int length){
	int i = 0;
	for (i; i < length; i++)
		fprintf(stdout, "0x%.2X ", data[i]);
}

static void validity_print_packet_long(unsigned char *data, int length){
	int i = 0;
	int z = 0;
	int c = 0;

	for (i; i < length; i++){
		fprintf(stdout, "%.2X ", data[i]);
		c++;
		if (c == 16){
			fprintf(stdout, "                       ");
			for (z = i - 16; z < i; z++)
				fprintf(stdout, "%c", data[i]);
			fprintf(stdout, "\n");
			c = 0;
		}
	}
	fprintf(stdout, "\nPrinted %d bytes", length);
}
/** Sends data to device 
 * if transfered < length - error
 */
static int validity_send_data(unsigned char *data, int length){
	validity_numerate_packet(data);
	fprintf(stdout, "UP  : ");
	validity_print_packet(data, length);
	fprintf(stdout, "\n");

	int transferred = 0;
	int r = libusb_bulk_transfer(devh, VALIDITY_SEND_ENDPOINT, data, length, &transferred, VALIDITY_DEFAULT_WAIT_TIMEOUT);	
	if (r < 0)
		return r;
	if (transferred < length)
		return r;
	return 0;
}

static int validity_receive_long_data(){
	int transferred = 0;
	unsigned char data[50000];
	int r = libusb_bulk_transfer(devh, VALIDITY_RECEIVE_ENDPOINT_LONG, data, 50000, &transferred, VALIDITY_DEFAULT_WAIT_TIMEOUT);
	if (r < 0)
		return r;
	fprintf(stdout, "DOWN: \n");
	validity_print_packet_long(data, transferred);
	fprintf(stdout, "\n");
	return 0;
}

static int validity_receive_data(){
	int transferred = 0;
	unsigned char data[50];
	int r = libusb_bulk_transfer(devh, VALIDITY_RECEIVE_ENDPOINT, data, 50, &transferred, VALIDITY_DEFAULT_WAIT_TIMEOUT);
	if (r < 0)
		return r;
	fprintf(stdout, "DOWN: ");
	validity_print_packet(data, transferred);
	fprintf(stdout, "\n");
	return 0;
}

static int validity_swap_messages(unsigned char *data, int length){
	int r = validity_send_data(data, length);
	if (r != 0)
		return r;
	usleep(25000);
	r = validity_receive_data();
	if (r != 0)
		return r;           
	return 0;
}

static int validity_reset_device(void){
	unsigned char data[] = "\x01\x00\x00\x00\x01\x00";
	int r = validity_send_data(data, (int) sizeof(data) - 1);
	return r;
}

static int validity_cycle3(void){
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
	unsigned char data17[] = "\x11\x00\x00\x00\x0E\x00\x00\x00";
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
	unsigned char data93[] = "\x5D\x00\x00\x00\x05\x00\x6D\x00\x32\x00";
	unsigned char data94[] = "\x5E\x00\x00\x00\x05\x00\x6E\x00\x03\x00";
	unsigned char data95[] = "\x5F\x00\x00\x00\x04\x00\x52\x00";
	unsigned char data96[] = "\x60\x00\x00\x00\x05\x00\x52\x00\x20\x03";
	unsigned char data97[] = "\x61\x00\x00\x00\x03\x00\x01\x00\x00\x00\x00\x00\x00\x01";
	unsigned char data98[] = "\x62\x00\x00\x00\x02\x00\x00";
	unsigned char data99[] = "\x63\x00\x00\x00\x05\x00\x52\x00\xB4\x1E";
	unsigned char data100[] = "\x64\x00\x00\x00\x05\x00\x55\x00\x08\x00";
	unsigned char data101[] = "\x65\x00\x00\x00\x12\x00\x2C\x50\xFF\x00\x02";
	unsigned char data102[] = "\x66\x00\x00\x00\x12\x00\x2E\x50\xFF\x00\x02";
	unsigned char data103[] = "\x67\x00\x00\x00\x13\x00\xF6\x05\x00\x00\x01\x00\x00\x00\x01";
	unsigned char data104[] = "\x68\x00\x00\x00\x12\x00\x3E\x50\xFF\x00\x01";
	unsigned char data105[] = "\x69\x00\x00\x00\x13\x00\x3E\x50\xFF\x00\x00\x00\x00\x00\x01";
	unsigned char data106[] = "\x6A\x00\x00\x00\x12\x00\x02\x98\xFF\x00\x01";
	unsigned char data107[] = "\x6B\x00\x00\x00\x12\x00\x00\x98\xFF\x00\x01";
	unsigned char data108[] = "\x6C\x00\x00\x00\x12\x00\x06\x98\xFF\x00\x01";
	unsigned char data109[] = "\x6D\x00\x00\x00\x13\x00\x06\x98\xFF\x00\x00\x00\x00\x00\x01";
	unsigned char data110[] = "\x6E\x00\x00\x00\x03\x00\x64\x00\x00\x01\x00\x00\x00\x01";
	unsigned char data111[] = "\x6F\x00\x00\x00\x13\x00\xF6\x05\x00\x00\x00\x00\x00\x00\x01";
	unsigned char data112[] = "\x70\x00\x00\x00\x12\x00\x3E\x50\xFF\x00\x01";
	unsigned char data113[] = "\x71\x00\x00\x00\x13\x00\x3E\x50\xFF\x00\x10\x00\x00\x00\x01";
	unsigned char data114[] = "\x72\x00\x00\x00\x12\x00\x02\x98\xFF\x00\x01";
	unsigned char data115[] = "\x73\x00\x00\x00\x12\x00\x00\x98\xFF\x00\x01";
	unsigned char data116[] = "\x74\x00\x00\x00\x12\x00\x06\x98\xFF\x00\x01";
	unsigned char data117[] = "\x75\x00\x00\x00\x03\x00\x64\x00\x00\x01\x00\x00\x00\x01";
	unsigned char data118[] = "\x76\x00\x00\x00\x12\x00\x38\x50\xFF\x00\x01";
	unsigned char data119[] = "\x77\x00\x00\x00\x12\x00\x0E\x50\xFF\x00\x02";
	unsigned char data120[] = "\x78\x00\x00\x00\x12\x00\x32\x50\xFF\x00\x01";
	unsigned char data121[] = "\x79\x00\x00\x00\x13\x00\x32\x50\xFF\x00\x12\x00\x00\x00\x01";
	unsigned char data122[] = "\x7A\x00\x00\x00\x13\x00\x0E\x50\xFF\x00\x00\x40\x00\x00\x02";
	unsigned char data123[] = "\x7B\x00\x00\x00\x13\x00\x38\x50\xFF\x00\x0F\x00\x00\x00\x01";
	unsigned char data124[] = "\x7C\x00\x00\x00\x05\x00\x62\x00\x00\x00";
	unsigned char data125[] = "\x7D\x00\x00\x00\x05\x00\x77\x00\x00\x00";
	unsigned char data126[] = "\x7E\x00\x00\x00\x05\x00\x76\x00\x00\x00";
	unsigned char data127[] = "\x7F\x00\x00\x00\x05\x00\x78\x00\x00\x00";
	unsigned char data128[] = "\x80\x00\x00\x00\x03\x00\x02\x00\x00\x01\x00\x00\x00\x01";
	unsigned char data129[] = "\x81\x00\x00\x00\x13\x00\xF6\x05\x00\x00\x01\x00\x00\x00\x01";
	unsigned char data130[] = "\x82\x00\x00\x00\x12\x00\x3E\x50\xFF\x00\x01";
	unsigned char data131[] = "\x83\x00\x00\x00\x13\x00\x3E\x50\xFF\x00\x00\x00\x00\x00\x01";
	unsigned char data132[] = "\x84\x00\x00\x00\x12\x00\x02\x98\xFF\x00\x01";
	unsigned char data133[] = "\x85\x00\x00\x00\x12\x00\x00\x98\xFF\x00\x01";
	unsigned char data134[] = "\x86\x00\x00\x00\x12\x00\x06\x98\xFF\x00\x01";
	unsigned char data135[] = "\x87\x00\x00\x00\x13\x00\x38\x50\xFF\x00\x0E\x00\x00\x00\x01";
	unsigned char data136[] = "\x88\x00\x00\x00\x03\x00\x0A\x00\x00\x01\x00\x00\x00\x01";
	unsigned char data137[] = "\x89\x00\x00\x00\x13\x00\x38\x50\xFF\x00\x0D\x00\x00\x00\x01";
	unsigned char data138[] = "\x8A\x00\x00\x00\x03\x00\x0A\x00\x00\x01\x00\x00\x00\x01";
	unsigned char data139[] = "\x8B\x00\x00\x00\x13\x00\x38\x50\xFF\x00\x0C\x00\x00\x00\x01";
	unsigned char data140[] = "\x8C\x00\x00\x00\x03\x00\x0A\x00\x00\x01\x00\x00\x00\x01";
	unsigned char data141[] = "\x8D\x00\x00\x00\x13\x00\x38\x50\xFF\x00\x0B\x00\x00\x00\x01";
	unsigned char data142[] = "\x8E\x00\x00\x00\x03\x00\x0A\x00\x00\x01\x00\x00\x00\x01";
	unsigned char data143[] = "\x8F\x00\x00\x00\x13\x00\x38\x50\xFF\x00\x0A\x00\x00\x00\x01";
	unsigned char data144[] = "\x90\x00\x00\x00\x03\x00\x0A\x00\x00\x01\x00\x00\x00\x01";
	unsigned char data145[] = "\x91\x00\x00\x00\x13\x00\x38\x50\xFF\x00\x09\x00\x00\x00\x01";
	unsigned char data146[] = "\x92\x00\x00\x00\x03\x00\x0A\x00\x00\x01\x00\x00\x00\x01";
	unsigned char data147[] = "\x93\x00\x00\x00\x13\x00\x38\x50\xFF\x00\x08\x00\x00\x00\x01";
	unsigned char data148[] = "\x94\x00\x00\x00\x03\x00\x0A\x00\x00\x01\x00\x00\x00\x01";
	unsigned char data149[] = "\x95\x00\x00\x00\x13\x00\x38\x50\xFF\x00\x07\x00\x00\x00\x01";
	unsigned char data150[] = "\x96\x00\x00\x00\x03\x00\x0A\x00\x00\x01\x00\x00\x00\x01";
	unsigned char data151[] = "\x97\x00\x00\x00\x13\x00\xF6\x05\x00\x00\x00\x00\x00\x00\x01";
	unsigned char data152[] = "\x98\x00\x00\x00\x12\x00\x3E\x50\xFF\x00\x01";
	unsigned char data153[] = "\x99\x00\x00\x00\x13\x00\x3E\x50\xFF\x00\x10\x00\x00\x00\x01";
	unsigned char data154[] = "\x9A\x00\x00\x00\x12\x00\x02\x98\xFF\x00\x01";
	unsigned char data155[] = "\x9B\x00\x00\x00\x12\x00\x00\x98\xFF\x00\x01";
	unsigned char data156[] = "\x9C\x00\x00\x00\x12\x00\x06\x98\xFF\x00\x01";
	unsigned char data157[] = "\x9D\x00\x00\x00\x13\x00\x06\x98\xFF\x00\x00\x00\x00\x00\x01";
	unsigned char data158[] = "\x9E\x00\x00\x00\x03\x00\x0A\x00\x00\x01\x00\x00\x00\x01";
	unsigned char data159[] = "\x9F\x00\x00\x00\x05\x00\x77\x00\x07\x00";
	unsigned char data160[] = "\xA0\x00\x00\x00\x05\x00\x76\x00\x12\x00";
	unsigned char data161[] = "\xA1\x00\x00\x00\x05\x00\x78\x00\x40\x21";
	unsigned char data162[] = "\xA2\x00\x00\x00\x13\x00\x38\x50\xFF\x00\x14\x00\x00\x00\x01";
	unsigned char data163[] = "\xA3\x00\x00\x00\x13\x00\x0E\x50\xFF\x00\xA4\x21\x00\x00\x02";
	unsigned char data164[] = "\xA4\x00\x00\x00\x13\x00\x32\x50\xFF\x00\x31\x00\x00\x00\x01";
	unsigned char data165[] = "\xA5\x00\x00\x00\x05\x00\x62\x00\x32\x00";
	unsigned char data166[] = "\xA6\x00\x00\x00\x0E\x00";
	unsigned char data167[] = "\xA7\x00\x00\x00\x05\x00\x62\x00\x32\x00";
	unsigned char data168[] = "\xA8\x00\x00\x00\x04\x00\x14\x00";
	unsigned char data169[] = "\xA9\x00\x00\x00\x04\x00\x11\x00";
	unsigned char data170[] = "\xAA\x00\x00\x00\x05\x00\x62\x00\x32\x00";
	unsigned char data171[] = "\xAB\x00\x00\x00\x03\x00\x14\x00\x00\x01\x00\x00\x00\x01";
	unsigned char data172[] = "\xAC\x00\x00\x00\x04\x00\x14\x00";
	unsigned char data173[] = "\xAD\x00\x00\x00\x04\x00\x14\x00";
	unsigned char data174[] = "\xAE\x00\x00\x00\x0E\x00";
	unsigned char data175[] = "\xAF\x00\x00\x00\x04\x00\x11\x00";
	unsigned char data176[] = "\xB0\x00\x00\x00\x05\x00\x62\x00\x32\x00";
	unsigned char data177[] = "\xB1\x00\x00\x00\x03\x00\x88\x13\x01\x00\x00\x00\x01\x01";
	unsigned char data178[] = "\xB2\x00\x00\x00\x16\x00";
        int r = 0;
	validity_swap_messages(data1, (int) sizeof(data1) - 1); 
	validity_swap_messages(data2, (int) sizeof(data2) - 1); 
	validity_swap_messages(data3, (int) sizeof(data3) - 1); 
	validity_swap_messages(data4, (int) sizeof(data4) - 1); 
	validity_swap_messages(data5, (int) sizeof(data5) - 1); 
	validity_swap_messages(data6, (int) sizeof(data6) - 1); 
	validity_swap_messages(data7, (int) sizeof(data7) - 1); 
	validity_swap_messages(data8, (int) sizeof(data8) - 1); 
	validity_swap_messages(data9, (int) sizeof(data9) - 1); 
	validity_swap_messages(data10, (int) sizeof(data10) - 1); 
	validity_swap_messages(data11, (int) sizeof(data11) - 1); 
	validity_swap_messages(data12, (int) sizeof(data12) - 1); 
	validity_swap_messages(data13, (int) sizeof(data13) - 1); 
	validity_swap_messages(data14, (int) sizeof(data14) - 1); 
	validity_swap_messages(data15, (int) sizeof(data15) - 1); 
	validity_swap_messages(data16, (int) sizeof(data16) - 1); 
	validity_swap_messages(data17, (int) sizeof(data17) - 1); 
	validity_swap_messages(data18, (int) sizeof(data18) - 1); 
	validity_swap_messages(data19, (int) sizeof(data19) - 1); 
	validity_swap_messages(data20, (int) sizeof(data20) - 1); 
	validity_swap_messages(data21, (int) sizeof(data21) - 1); 
	validity_swap_messages(data22, (int) sizeof(data22) - 1); 
	validity_swap_messages(data23, (int) sizeof(data23) - 1); 
	validity_swap_messages(data24, (int) sizeof(data24) - 1); 
	validity_swap_messages(data25, (int) sizeof(data25) - 1); 
	validity_swap_messages(data26, (int) sizeof(data26) - 1); 
	validity_swap_messages(data27, (int) sizeof(data27) - 1); 
	validity_swap_messages(data28, (int) sizeof(data28) - 1); 
	validity_swap_messages(data29, (int) sizeof(data29) - 1); 
	validity_swap_messages(data30, (int) sizeof(data30) - 1); 
	validity_swap_messages(data31, (int) sizeof(data31) - 1); 
	validity_swap_messages(data32, (int) sizeof(data32) - 1); 
	validity_swap_messages(data33, (int) sizeof(data33) - 1); 
	validity_swap_messages(data34, (int) sizeof(data34) - 1); 
	validity_swap_messages(data35, (int) sizeof(data35) - 1); 
	validity_swap_messages(data36, (int) sizeof(data36) - 1); 
	validity_swap_messages(data37, (int) sizeof(data37) - 1); 
	validity_swap_messages(data38, (int) sizeof(data38) - 1); 
	validity_swap_messages(data39, (int) sizeof(data39) - 1); 
	validity_swap_messages(data40, (int) sizeof(data40) - 1); 
	validity_swap_messages(data41, (int) sizeof(data41) - 1); 
	validity_swap_messages(data42, (int) sizeof(data42) - 1); 
	validity_swap_messages(data43, (int) sizeof(data43) - 1); 
	validity_swap_messages(data44, (int) sizeof(data44) - 1); 
	validity_swap_messages(data45, (int) sizeof(data45) - 1); 
	validity_swap_messages(data46, (int) sizeof(data46) - 1); 
	validity_swap_messages(data47, (int) sizeof(data47) - 1); 
	validity_swap_messages(data48, (int) sizeof(data48) - 1); 
	validity_swap_messages(data49, (int) sizeof(data49) - 1); 
	validity_swap_messages(data50, (int) sizeof(data50) - 1); 
	validity_swap_messages(data51, (int) sizeof(data51) - 1); 
	validity_swap_messages(data52, (int) sizeof(data52) - 1); 
	validity_swap_messages(data53, (int) sizeof(data53) - 1); 
	validity_swap_messages(data54, (int) sizeof(data54) - 1); 
	validity_swap_messages(data55, (int) sizeof(data55) - 1); 
	validity_swap_messages(data56, (int) sizeof(data56) - 1); 
	validity_swap_messages(data57, (int) sizeof(data57) - 1); 
	validity_swap_messages(data58, (int) sizeof(data58) - 1); 
	validity_swap_messages(data59, (int) sizeof(data59) - 1); 
	validity_swap_messages(data60, (int) sizeof(data60) - 1); 
	validity_swap_messages(data61, (int) sizeof(data61) - 1); 
	validity_swap_messages(data62, (int) sizeof(data62) - 1); 
	validity_swap_messages(data63, (int) sizeof(data63) - 1); 
	validity_swap_messages(data64, (int) sizeof(data64) - 1); 
	validity_swap_messages(data65, (int) sizeof(data65) - 1); 
	validity_swap_messages(data66, (int) sizeof(data66) - 1); 
	validity_swap_messages(data67, (int) sizeof(data67) - 1); 
	validity_swap_messages(data68, (int) sizeof(data68) - 1); 
	validity_swap_messages(data69, (int) sizeof(data69) - 1); 
	validity_swap_messages(data70, (int) sizeof(data70) - 1); 
	validity_swap_messages(data71, (int) sizeof(data71) - 1); 
	validity_swap_messages(data72, (int) sizeof(data72) - 1); 
	validity_swap_messages(data73, (int) sizeof(data73) - 1); 
	validity_swap_messages(data74, (int) sizeof(data74) - 1); 
	validity_swap_messages(data75, (int) sizeof(data75) - 1); 
	validity_swap_messages(data76, (int) sizeof(data76) - 1); 
	validity_swap_messages(data77, (int) sizeof(data77) - 1); 
	validity_swap_messages(data78, (int) sizeof(data78) - 1); 
	validity_swap_messages(data79, (int) sizeof(data79) - 1); 
	validity_swap_messages(data80, (int) sizeof(data80) - 1); 
	validity_swap_messages(data81, (int) sizeof(data81) - 1); 
	validity_swap_messages(data82, (int) sizeof(data82) - 1); 
	validity_swap_messages(data83, (int) sizeof(data83) - 1); 
	validity_swap_messages(data84, (int) sizeof(data84) - 1); 
	validity_swap_messages(data85, (int) sizeof(data85) - 1); 
	validity_swap_messages(data86, (int) sizeof(data86) - 1); 
	validity_swap_messages(data87, (int) sizeof(data87) - 1); 
	validity_swap_messages(data88, (int) sizeof(data88) - 1); 
	validity_swap_messages(data89, (int) sizeof(data89) - 1); 
	validity_swap_messages(data90, (int) sizeof(data90) - 1); 
	validity_swap_messages(data91, (int) sizeof(data91) - 1); 
	validity_swap_messages(data92, (int) sizeof(data92) - 1); 
	validity_swap_messages(data93, (int) sizeof(data93) - 1); 
	validity_swap_messages(data94, (int) sizeof(data94) - 1); 
	validity_swap_messages(data95, (int) sizeof(data95) - 1); 
	validity_swap_messages(data96, (int) sizeof(data96) - 1); 
	validity_swap_messages(data97, (int) sizeof(data97) - 1); 
	r = validity_receive_long_data();
	validity_swap_messages(data98, (int) sizeof(data98) - 1); 
	validity_swap_messages(data99, (int) sizeof(data99) - 1); 
	validity_swap_messages(data100, (int) sizeof(data100) - 1); 
	validity_swap_messages(data101, (int) sizeof(data101) - 1); 
	validity_swap_messages(data102, (int) sizeof(data102) - 1); 
	validity_swap_messages(data103, (int) sizeof(data103) - 1); 
	validity_swap_messages(data104, (int) sizeof(data104) - 1); 
	validity_swap_messages(data105, (int) sizeof(data105) - 1); 
	validity_swap_messages(data106, (int) sizeof(data106) - 1); 
	validity_swap_messages(data107, (int) sizeof(data107) - 1); 
	validity_swap_messages(data108, (int) sizeof(data108) - 1); 
	validity_swap_messages(data109, (int) sizeof(data109) - 1); 
	validity_swap_messages(data110, (int) sizeof(data110) - 1); 
	r = validity_receive_long_data();
	validity_swap_messages(data111, (int) sizeof(data111) - 1); 
	validity_swap_messages(data112, (int) sizeof(data112) - 1); 
	validity_swap_messages(data113, (int) sizeof(data113) - 1); 
	validity_swap_messages(data114, (int) sizeof(data114) - 1); 
	validity_swap_messages(data115, (int) sizeof(data115) - 1); 
	validity_swap_messages(data116, (int) sizeof(data116) - 1); 
	validity_swap_messages(data117, (int) sizeof(data117) - 1); 
	r = validity_receive_long_data();
	validity_swap_messages(data118, (int) sizeof(data118) - 1); 
	validity_swap_messages(data119, (int) sizeof(data119) - 1); 
	validity_swap_messages(data120, (int) sizeof(data120) - 1); 
	validity_swap_messages(data121, (int) sizeof(data121) - 1); 
	validity_swap_messages(data122, (int) sizeof(data122) - 1); 
	validity_swap_messages(data123, (int) sizeof(data123) - 1); 
	validity_swap_messages(data124, (int) sizeof(data124) - 1); 
	validity_swap_messages(data125, (int) sizeof(data125) - 1); 
	validity_swap_messages(data126, (int) sizeof(data126) - 1); 
	validity_swap_messages(data127, (int) sizeof(data127) - 1); 
	validity_swap_messages(data128, (int) sizeof(data128) - 1); 
	r = validity_receive_long_data();
	validity_swap_messages(data129, (int) sizeof(data129) - 1); 
	validity_swap_messages(data130, (int) sizeof(data130) - 1); 
	validity_swap_messages(data131, (int) sizeof(data131) - 1); 
	validity_swap_messages(data132, (int) sizeof(data132) - 1); 
	validity_swap_messages(data133, (int) sizeof(data133) - 1); 
	validity_swap_messages(data134, (int) sizeof(data134) - 1); 
	validity_swap_messages(data135, (int) sizeof(data135) - 1); 
	validity_swap_messages(data136, (int) sizeof(data136) - 1); 
	r = validity_receive_long_data();
	validity_swap_messages(data137, (int) sizeof(data137) - 1); 
	validity_swap_messages(data138, (int) sizeof(data138) - 1); 
	r = validity_receive_long_data();
	validity_swap_messages(data139, (int) sizeof(data139) - 1); 
	validity_swap_messages(data140, (int) sizeof(data140) - 1); 
	r = validity_receive_long_data();
	validity_swap_messages(data141, (int) sizeof(data141) - 1); 
	validity_swap_messages(data142, (int) sizeof(data142) - 1); 
	r = validity_receive_long_data();
	validity_swap_messages(data143, (int) sizeof(data143) - 1); 
	validity_swap_messages(data144, (int) sizeof(data144) - 1); 
	r = validity_receive_long_data();
	validity_swap_messages(data145, (int) sizeof(data145) - 1); 
	validity_swap_messages(data146, (int) sizeof(data146) - 1); 
	r = validity_receive_long_data();
	validity_swap_messages(data147, (int) sizeof(data147) - 1); 
	validity_swap_messages(data148, (int) sizeof(data148) - 1); 
	r = validity_receive_long_data();
	validity_swap_messages(data149, (int) sizeof(data149) - 1); 
	validity_swap_messages(data150, (int) sizeof(data150) - 1); 
	r = validity_receive_long_data();
	validity_swap_messages(data151, (int) sizeof(data151) - 1); 
	validity_swap_messages(data152, (int) sizeof(data152) - 1); 
	validity_swap_messages(data153, (int) sizeof(data153) - 1); 
	validity_swap_messages(data154, (int) sizeof(data154) - 1); 
	validity_swap_messages(data155, (int) sizeof(data155) - 1); 
	validity_swap_messages(data156, (int) sizeof(data156) - 1); 
	validity_swap_messages(data157, (int) sizeof(data157) - 1); 
	validity_swap_messages(data158, (int) sizeof(data158) - 1); 
	r = validity_receive_long_data();
	validity_swap_messages(data159, (int) sizeof(data159) - 1); 
	validity_swap_messages(data160, (int) sizeof(data160) - 1); 
	validity_swap_messages(data161, (int) sizeof(data161) - 1); 
	validity_swap_messages(data162, (int) sizeof(data162) - 1); 
	validity_swap_messages(data163, (int) sizeof(data163) - 1); 
	validity_swap_messages(data164, (int) sizeof(data164) - 1); 
	validity_swap_messages(data165, (int) sizeof(data165) - 1); 
	validity_swap_messages(data166, (int) sizeof(data166) - 1); 
	validity_swap_messages(data167, (int) sizeof(data167) - 1); 
	validity_swap_messages(data168, (int) sizeof(data168) - 1); 
	validity_swap_messages(data169, (int) sizeof(data169) - 1); 
	validity_swap_messages(data170, (int) sizeof(data170) - 1); 
	validity_swap_messages(data171, (int) sizeof(data171) - 1); 
	r = validity_receive_long_data();
	validity_swap_messages(data172, (int) sizeof(data172) - 1); 
	validity_swap_messages(data173, (int) sizeof(data173) - 1); 
	validity_swap_messages(data174, (int) sizeof(data174) - 1); 
	validity_swap_messages(data175, (int) sizeof(data175) - 1); 
	validity_swap_messages(data176, (int) sizeof(data176) - 1); 
	validity_swap_messages(data177, (int) sizeof(data177) - 1); 

	int i = 0;
	for (i; i < 500; i++)
		validity_swap_messages(data178, (int) sizeof(data178) - 1); 
		validity_receive_long_data();
	return 0;
}

static int validity_cycle2(void){
	unsigned char data00 [] = "\x01\x00\x00\x00\x12\x00\xE8\x1F\x00\x00\x04";
	unsigned char data01 [] = "\x02\x00\x00\x00\x12\x00\xEC\x1F\x00\x00\x04";
	unsigned char data02 [] = "\x03\x00\x00\x00\x12\x00\xF0\x1F\x00\x00\x04";
	unsigned char data03 [] = "\x04\x00\x00\x00\x12\x00\xF4\x1F\x00\x00\x04";
	unsigned char data04 [] = "\x05\x00\x00\x00\x12\x00\xF8\x1F\x00\x00\x04";
	unsigned char data05 [] = "\x06\x00\x00\x00\x12\x00\xFC\x1F\x00\x00\x04";
	unsigned char data[] = "\xF5\x00\x00\x00\x16\x00"; 
	
	validity_swap_messages(data00, (int) sizeof(data00) - 1);
	validity_swap_messages(data01, (int) sizeof(data01) - 1);
	validity_swap_messages(data02, (int) sizeof(data02) - 1);
	validity_swap_messages(data03, (int) sizeof(data03) - 1);
	validity_swap_messages(data04, (int) sizeof(data04) - 1);
	validity_swap_messages(data05, (int) sizeof(data05) - 1);

	int r = validity_swap_messages(data, (int) sizeof(data) - 1);
	return r;
}

static int validity_cycle1(void){
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

	int r = validity_swap_messages(data00, (int) sizeof(data00) - 1);
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(data01, (int) sizeof(data01) - 1);
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(data02, (int) sizeof(data02) - 1);
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(data03, (int) sizeof(data03) - 1);
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(data04, (int) sizeof(data04) - 1);
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(data05, (int) sizeof(data05) - 1);
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(data06, (int) sizeof(data06) - 1);
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(data07, (int) sizeof(data07) - 1);
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(data08, (int) sizeof(data08) - 1);
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(data09, (int) sizeof(data09) - 1);
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(data10, (int) sizeof(data10) - 1);
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(data11, (int) sizeof(data11) - 1);
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(data12, (int) sizeof(data12) - 1);
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(data13, (int) sizeof(data13) - 1);
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(data14, (int) sizeof(data14) - 1);
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(data15, (int) sizeof(data15) - 1);
	if (r != 0)                                               
		return r;                                         

	r = validity_receive_long_data();

	r = validity_swap_messages(data16, (int) sizeof(data16) - 1);
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(data17, (int) sizeof(data17) - 1);
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(data18, (int) sizeof(data18) - 1);
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(data19, (int) sizeof(data19) - 1);
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(data20, (int) sizeof(data20) - 1);
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(data21, (int) sizeof(data21) - 1);
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(data22, (int) sizeof(data22) - 1);
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(data23, (int) sizeof(data23) - 1);
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(data24, (int) sizeof(data24) - 1);
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(data25, (int) sizeof(data25) - 1);
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(data26, (int) sizeof(data26) - 1);
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(data27, (int) sizeof(data27) - 1);
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(data28, (int) sizeof(data28) - 1);
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(data29, (int) sizeof(data29) - 1);
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(data30, (int) sizeof(data30) - 1);
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(data31, (int) sizeof(data31) - 1);
	if (r != 0)                                               
		return r;                                         

	r = validity_swap_messages(data32, (int) sizeof(data32) - 1);
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(data33, (int) sizeof(data33) - 1);
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(data34, (int) sizeof(data34) - 1);
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(data35, (int) sizeof(data35) - 1);
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(data36, (int) sizeof(data36) - 1);
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(data37, (int) sizeof(data37) - 1);
	if (r != 0)                                               
		return r;                                         

	int i = 0;
	for (i; i < 80; i++){
		r = validity_swap_messages(data4, (int) sizeof(data4) - 1);
		if (r != 0)
			return r;
	}	
/*	
	r = validity_swap_messages(data32, (int) sizeof(data32- 1));
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(data33, (int) sizeof(data33- 1));
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(data34, (int) sizeof(data34- 1));
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(data35, (int) sizeof(data35- 1));
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(data36, (int) sizeof(data36- 1));
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(data37, (int) sizeof(data37- 1));
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(data38, (int) sizeof(data38- 1));
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(data39, (int) sizeof(data39- 1));
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(data40, (int) sizeof(data40- 1));
	if (r != 0)                                               
		return r;                                         
	r = validity_swap_messages(data41, (int) sizeof(data41- 1));
	if (r != 0)
		return r;
*/

	return 0;
}

static int validity_cycle0(void){
	unsigned char data1[] = "\x01\x00\x00\x00\x12\x00\xE8\x1F\x00\x00\x04";
	unsigned char data2[] = "\x02\x00\x00\x00\x12\x00\xEC\x1F\x00\x00\x04";
	unsigned char data3[] = "\x03\x00\x00\x00\x12\x00\xF0\x1F\x00\x00\x04";
	unsigned char data4[] = "\x04\x00\x00\x00\x12\x00\xF4\x1F\x00\x00\x04";
	unsigned char data5[] = "\x05\x00\x00\x00\x12\x00\xF8\x1F\x00\x00\x04";
	unsigned char data6[] = "\x06\x00\x00\x00\x12\x00\xFC\x1F\x00\x00\x04";


	validity_swap_messages(data1, (int) sizeof(data1) - 1); 
	validity_swap_messages(data2, (int) sizeof(data2) - 1); 
	validity_swap_messages(data3, (int) sizeof(data3) - 1); 
	validity_swap_messages(data4, (int) sizeof(data4) - 1); 
	validity_swap_messages(data5, (int) sizeof(data5) - 1); 
	validity_swap_messages(data6, (int) sizeof(data6) - 1); 


	unsigned char data36 [] = "\x04\x65\x00\x00\x05\x00\x62\x00\x32\x00";
	unsigned char data171[] = "\xAB\x00\x00\x00\x03\x00\x14\x00\x00\x01\x00\x00\x00\x01";
	unsigned char data44[] = "\xF5\x00\x00\x00\x16\x00"; 

	validity_swap_messages(data36, (int) sizeof(data36) - 1);
	validity_swap_messages(data171, (int) sizeof(data171) - 1);
//	validity_receive_long_data();	

	int i = 0;

	for (i; i < 50; i++){
		validity_swap_messages(data44, (int) sizeof(data44) - 1);
		validity_receive_long_data();
	}

	return 0; 
}
	
static int validity_cycle(void){
	unsigned char data[] = "\x00\x00\x00\x00\x12\x00\xE8\x1F\x00\x00\x04";//{packet_num_l, packet_num_h, 0x00, 0x00, 0x12, 0x00, 0xE8, 0x1F, 0x00, 0x00, 0x04};
	unsigned char data1[] = "\x02\x00\x00\x00\x12\x00\xEC\x1F\x00\x00\x04";
	unsigned char data2[] = "\x10\x00\x00\x00\x03\x00\x01\x00\x00\x01\x00\x00\x00\x01";
	unsigned char data3[] = "\xA4\x00\x00\x00\x0E\x00";
	unsigned char data4[] = "\xF5\x00\x00\x00\x16\x00"; 
	unsigned char data5[] = "\xB1\x00\x00\x00\x05\x00\x62\x00\x32\x00"; 
	unsigned char data6[] = "\xB2\x00\x00\x00\x03\x00\x88\x13\x01\x00\x00\x00\x01\x01"; 
	unsigned char data7[] = "\xB0\x00\x00\x00\x04\x00\x11\x00";

	int r = validity_swap_messages(data, (int) sizeof(data) - 1);
	if (r != 0)
		return r;
	r = validity_swap_messages(data1, (int) sizeof(data1) - 1);
	if (r != 0)
		return r;
	r = validity_swap_messages(data2, (int) sizeof(data2) - 1);
	if (r != 0)
		return r;
	r = validity_receive_long_data();

	r = validity_swap_messages(data3, (int) sizeof(data3) - 1);
	if (r != 0)
		return r;

	r = validity_swap_messages(data7, (int) sizeof(data7) - 1);
	if (r != 0)
		return r;

	r = validity_swap_messages(data5, (int) sizeof(data5) - 1);
	if (r != 0)
		return r;

	r = validity_swap_messages(data6, (int) sizeof(data6) - 1);
	if (r != 0)
		return r;

	int i = 0;
	for (i; i < 20; i++){
		r = validity_swap_messages(data4, (int) sizeof(data4) - 1);
		if (r != 0)
			return r;
	}	

	return 0;	
}

/** Main function */
int main(void)
{
	struct sigaction sigact;
	int r = 1;

	r = libusb_init(&ctx);
	if (r < 0) {
		fprintf(stderr, "failed to initialise libusb\n");
		exit(1);
	}
	fprintf(stdout, "Libusb inititalized!\n");


	fprintf(stdout, "Searching for device...\n");
	r = validity_find_device();
	if (r != 0){
		fprintf(stderr, "Can't find validity device!\n");
		goto out;
	}
	fprintf(stdout, "Device found!\n");


	fprintf(stdout, "Claiming interface...\n");
	r = libusb_claim_interface(devh, 0);
	if (r < 0) {
		fprintf(stderr, "usb_claim_interface error %d\n", r);
		goto out;
	}
	fprintf(stdout, "claimed interface\n");


	fprintf(stdout, "Resetting device\n");
	r = validity_reset_device();
	if (r < 0) {
		fprintf(stderr, "Error resetting device %d\n", r);
		goto out;
	}
	fprintf(stdout, "Device resetted. Begin initialization...\n");

	usleep(2000000);
	
	libusb_release_interface(devh, 0);
	libusb_reset_device(devh); 
	libusb_close(devh);

	fprintf(stdout, "Searching for device...\n");
	r = validity_find_device();
	if (r != 0){
		fprintf(stderr, "Can't find validity device!\n");
		goto out;
	}
	fprintf(stdout, "Device found!\n");


	fprintf(stdout, "Claiming interface...\n");
	r = libusb_claim_interface(devh, 0);
	if (r < 0) {
		fprintf(stderr, "usb_claim_interface error %d\n", r);
		goto out;
	}
	fprintf(stdout, "claimed interface\n");
/*
	r = 
	if (r < 0) {
		fprintf(stderr, "Error resetting device");
		goto out_release;
	}
	fprintf(stdout, "Device reset success!");
	goto out;
*/

	fprintf(stdout, "Configuring device...\n");
	r = validity_configure_device();
        if (r < 0) {
		fprintf(stderr, "device configuring error %d\n", r);
		goto out_release;
	}
	fprintf(stdout, "Device configured!\n");

	fprintf(stdout, "Entering main cycle...\n");
	r = validity_cycle3();
	if (r != 0) {
		fprintf(stderr, "got error in main cycle %d\n", r);
		goto out_release;
	}
	fprintf(stdout, "Main cycle complete success!\n");

out_release:
	libusb_release_interface(devh, 0);
	libusb_reset_device(devh); 
	libusb_close(devh);
out:
	libusb_exit(ctx);
	return r;
}
