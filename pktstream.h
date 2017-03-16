#ifndef PKTSTREAM_H
#define PKTSTREAM_H


#define DEVICE_NAME "pktstrm"
#define MAJOR_NUM 75
#define MAX_PKT_SIZE 4096
#define MAX_FILE_SIZE 4194304
#define PKT_DEFAULT_SIZE 256
#define FILE_DEFAULT_SIZE 262144
#define DEVICE_GENERAL_LOCK -1

#define PKTSTRM_IOCTL_SET_MODE_PACKET _IO(MAJOR_NUM, 0)
#define PKTSTRM_IOCTL_SET_MODE_STREAM _IO(MAJOR_NUM, 1)
#define PKTSTRM_IOCTL_SET_ACC_BLOCK _IO(MAJOR_NUM, 2)
#define PKTSTRM_IOCTL_SET_ACC_NO_BLOCK _IO(MAJOR_NUM, 3)
#define PKTSTRM_IOCTL_SET_PKT_SIZE _IOW(MAJOR_NUM, 4, size_t)
#define PKTSTRM_IOCTL_SET_FILE_SIZE _IOW(MAJOR_NUM, 5, size_t)

typedef unsigned char byte;

typedef enum {PACKET, STREAM} device_mode;
typedef enum {NON_BLOCK, BLOCK} access_mode;


/*
 * Data structures used by the module
 */

typedef struct segment {
	// current segment size
	size_t segment_size;
	
	// pointer to the next segment in the linked list
	struct segment * next;

	// pointer to the actual data
	byte * segment_buffer;
} segment;

typedef struct minor_file {
	// number of clients using this minor
	unsigned int clients;

	// current amount of data bytes maintained in segments
	size_t data_count;

	// current default segment size
	size_t def_segment_size;

	// current maximum file size
	size_t file_size;

	// semaphore for both read and write access
	struct mutex rw_access;

	// wait queue for reading access
	wait_queue_head_t read_queue;
	
	// wait queue for writing access
	wait_queue_head_t write_queue;

	// operational mode of the file
	device_mode op_mode;

	// access mode of the file
	access_mode ac_mode;

	// pointer to the first data segment in the minor file
	segment * first_segment;

	// pointer to the last data segment in the minor file
	segment * last_segment;
} minor_file;


#endif
