#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include "pktstream_lib.h"

#define BUF_SIZE 4096


/*
 * Testing utility for the pktstream device driver
*/

int fd0;
int fd1;
pthread_t t1; 
pthread_t t2; 


/**
 * Helper functions
 * */
void read_files(char * read_char) {
	int read_size;
	read_size = read(fd0, read_char, BUF_SIZE);
	printf("Bytes read: %d, Content: %s\n", read_size, read_char);
	memset(read_char, 0, BUF_SIZE);
	read_size = read(fd1, read_char, BUF_SIZE);
	printf("Bytes read: %d, Content: %s\n", read_size, read_char);
	memset(read_char, 0, BUF_SIZE); 
}

void write_files(char * to_write, int size) {
	int write_size;
	printf("Attempting to write %d bytes\n", size);
	write_size = write(fd0, to_write, size);
	printf("Bytes written: %d\n", write_size);
	write_size = write(fd1, to_write, size);
	printf("Bytes written: %d\n", write_size);

}



/** 
 * test the device in packet mode
 * */
void test_packet(char *to_write, int size, char *read_char){
	printf("------------------------------------------------------------\n");
	printf("Testing device in packet mode\n");
	write_files(to_write, size);
	read_files(read_char);
	read_files(read_char);
}



/** 
 * test the device in packet mode
 * */
void test_stream(char *to_write, int size, char *read_char){
	printf("------------------------------------------------------------\n");
	printf("Testing device in stream mode\n");
	write_files(to_write, size);
	read_files(read_char);
}


/**
 * test device cuncurrent access
 * */
void *test_cuncurrency(void * to_write){
	int size;
	int i;
	int write_size;
	char * message = (char *) to_write;
	size = strlen(message);
	
	for (i = 0; i < 10; i++){
		write_size = write(fd0, to_write, size);
	}

	return NULL;
}

void read_to_empty(char * read_char){
	int read_size;
	do {
		read_size = read(fd0, read_char, BUF_SIZE);
		printf("Bytes read: %d, Content: %s\n", read_size, read_char);
		memset(read_char, 0, BUF_SIZE);
	
		} while(read_size > 0);

}


int main() {
	int read_size;
	int write_size;
	int loerm_size;
	int err;
	char * read_char;
	char * to_write1;
	char * to_write2;
	char * lorem;

	lorem = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.";
	loerm_size = strlen(lorem);

	read_char = malloc(BUF_SIZE);
	
	to_write1 = "test1 ";
	to_write2 = "test2 ";

	// opening device files with minor numbers 0 and 1
	fd0 = open("devfile", O_RDWR);
	fd1 = open("devfile1", O_RDWR);
	printf("file descriptors: %d - %d\n", fd0, fd1);
	
	test_packet(lorem, loerm_size, read_char);
	set_mode_stream(fd0);
	set_mode_stream(fd1);
	test_stream(lorem, loerm_size, read_char);



	printf("------------------------------------------------------------\n");
	printf("Testing device in cuncurrent access\n");

	set_mode_packet(fd0);
	err = pthread_create(&t1, NULL, &test_cuncurrency, to_write1);
	if (err != 0)
	    printf("can't create thread :[%s]\n", strerror(err));
	err = pthread_create(&t2, NULL, &test_cuncurrency, to_write2);
	if (err != 0)
	    printf("can't create thread :[%s]\n", strerror(err));
	
	pthread_join(t1, NULL);
	pthread_join(t2, NULL); 

	read_to_empty(read_char);
	
	close(fd0);
	close(fd1);
	return 0;
}
