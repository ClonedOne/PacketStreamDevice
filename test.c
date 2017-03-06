#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#define BUF_SIZE 4096


/*
 * Testing utility for the pktstream device driver
 */

void read_files(int read_size, char * read_char, int fd0, int fd1) {
	read_size = read(fd0, read_char, 2);
	printf("Bytes read: %d, Content: %s\n", read_size, read_char);
	memset(read_char, 0, BUF_SIZE);
/*	read_size = read(fd1, read_char, 2);
	printf("Bytes read: %d, Content: %s\n", read_size, read_char);
	memset(read_char, 0, BUF_SIZE); */
}

void write_files(int write_size, char * to_write, int fd0, int fd1, int size) {
	write_size = write(fd0, to_write, size);
	printf("Bytes written: %d\n", write_size);
/*	write_size = write(fd1, to_write, size);
	printf("Bytes written: %d\n", write_size);
*/
}

int main() {
	int fd0;
	int fd1;
	int read_size;
	int write_size;
	char * read_char;
	char * to_write;
	int size;

	read_char = malloc(BUF_SIZE);
	// char write_char = 'c';
	// to_write = &write_char;
	to_write = "test";
	size = strlen(to_write);

	// opening device files with minor numbers 0 and 1
	fd0 = open("devfile", O_RDWR);
	fd1 = open("devfile1", O_RDWR);
	printf("file descriptors: %d - %d\n", fd0, fd1);
	
	read_files(read_size, read_char, fd0, fd1);
	write_files(write_size, to_write, fd0, fd1, size);
	read_files(read_size, read_char, fd0, fd1);
	read_files(read_size, read_char, fd0, fd1);

	close(fd0);
	close(fd1);
	return 0;
}
