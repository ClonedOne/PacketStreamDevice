#ifndef PKTSTREAM_LIB_H
#define PKTSTREAM_LIB_H

#include "pktstream.h"

void set_mode_packet(int);
void set_mode_stream(int);

void set_access_blocking(int);
void set_access_non_blocking(int);

int set_file_size(int, unsigned long);
int set_packet_size(int, unsigned long);


#endif
