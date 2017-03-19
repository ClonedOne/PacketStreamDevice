#ifndef PKTSTREAM_H
#define PKTSTREAM_H

#include <linux/ioctl.h>

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


#endif
