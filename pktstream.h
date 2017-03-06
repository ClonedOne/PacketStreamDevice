#ifndef PKTSTREAM_H
#define PKTSTREAM_H


#define DEVICE_NAME "pktstrm"
#define MAJOR_NUM 75
#define MAX_FILE_SIZE 4194304
#define PKT_DEFAULT_SIZE 2

typedef unsigned char byte;

typedef enum {PACKET, STREAM} device_mode;

#endif
