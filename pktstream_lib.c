#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "pktstream_lib.h"

/** 
 * modify the device operating mode
 *  - packet: read one packet for operation, discard non fitting bytes
 *  - stream: read up to capacity, split segments will become a new packet 
 * */
void set_mode_packet(int fd){
	ioctl(fd, PKTSTRM_IOCTL_SET_MODE_PACKET);
}

void set_mode_stream(int fd){
	ioctl(fd, PKTSTRM_IOCTL_SET_MODE_STREAM);
}



/** 
 * modify access mode in read/write operations
 * - blocking: wait for data/space until available or interrupted by signal
 * - non blocking: return immediately if no data/space is available
 * */
void set_access_blocking(int fd){
	ioctl(fd, PKTSTRM_IOCTL_SET_ACC_BLOCK);
}

void set_access_non_blocking(int fd){
	ioctl(fd, PKTSTRM_IOCTL_SET_ACC_NO_BLOCK);
}



/** set file and packet sizes */
int set_file_size(int fd, unsigned long size){
	if(ioctl(fd, PKTSTRM_IOCTL_SET_FILE_SIZE, size) == 0)
		return 0;
	printf("illegal specified size %zd", size);
	return -1;
}

int set_packet_size(int fd, unsigned long size){
	if(ioctl(fd, PKTSTRM_IOCTL_SET_PKT_SIZE, size) == 0)
		return 0;
	printf("illegal specified size %zd", size);
	return -1;
}

