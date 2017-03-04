#define EXPORT_SYMTAB
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/sched.h>	
#include <linux/pid.h>	
#include <linux/tty.h>
#include <linux/version.h>
#include <asm/uaccess.h>
#include "pktstream.h"



/*
 * Multi-mode packet stream device file.
 * This driver provides a FIFO device file accessible as either a stream device
 * or a packet device.
 */

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Giorgio Severi");
MODULE_DESCRIPTION("Multi-mode packet stream device file");
MODULE_VERSION("0.1");



/*
 * Data structures used by the module
 */

typedef struct segment {
	// current segment size
	int segment_size;
	
	// pointer to the next segment in the linked list
	struct segment * next;

	// pointer to the actual data
	byte * segment_buffer;
} segment;

typedef struct minor_file {
	// number of clients using this minor
	unsigned int clients;

	// current amount of data bytes maintained in segments
	unsigned int data_count;

	// pointer to the first data segment in the minor file
	segment * first_segment;

	// pointer to the last data segment in the minor file
	segment * last_segment;
} minor_file;



/*
 * Global variables for the module
 */

// array of pointers to minor_file data structures
static minor_file * minor_files[256] = {NULL};

// size of data segments
unsigned int pkt_size; 

// default operative mode
device_mode op_mode = PACKET;

byte * pktstream_buffer;



/*
 * Function declarations
 */

int pktstream_open(struct inode *node, struct file *file_p);

int pktstream_release(struct inode *node, struct file *file_p);

ssize_t pktstream_read(struct file *file_p, char *buff, size_t count, loff_t *f_pos);

ssize_t pktstream_write(struct file *file_p, char *buff, size_t count, loff_t *f_pos);

void pktstream_exit(void);

int pktstream_init(void);

void create_append_segments(minor_file * current_minor, unsigned int cur_size, byte * tmp);

int retrieve_minor_number(struct file *file_p, char * operation);

void print_bytes(byte * buff, unsigned int cur_size);



/*
 * Struct declaring the file access functions
 */

struct file_operations pktstream_fops = {
	.read = pktstream_read,
	.write = pktstream_write,
	.open = pktstream_open,
	.release = pktstream_release
};



/*
 * Declaration of mandatory init and exit functions
 */

module_init(pktstream_init);

module_exit(pktstream_exit);



/*
 * Module life-cycle utilities
 */

int pktstream_init(void) {
	int major_num;
	pkt_size = PKT_DEFAULT_SIZE;
	
	// Try to register device major number
	major_num = register_chrdev(MAJOR_NUM, DEVICE_NAME, &pktstream_fops); 
	if (major_num < 0){
		printk(KERN_ALERT "%s: cannot obtain major number %d\n", DEVICE_NAME, MAJOR_NUM);
		return major_num;
	}
	printk(KERN_INFO "%s: registered correctly with major number %d\n",DEVICE_NAME, MAJOR_NUM);	

	// Try to allocate memory for the buffer
	pktstream_buffer = kmalloc(1, GFP_KERNEL);
	if (!pktstream_buffer){
		major_num = -ENOMEM;
		pktstream_exit();
		return major_num;
	}

	memset(pktstream_buffer, 0, 1);
	printk(KERN_INFO "inserting module: %s\n", DEVICE_NAME);

	return 0;
}

void pktstream_exit(void){
	unregister_chrdev(MAJOR_NUM, DEVICE_NAME);
	if (pktstream_buffer)
		kfree(pktstream_buffer);
	printk(KERN_INFO "removing module: %s\n", DEVICE_NAME);
}



/*
 * Module open and release
 */

int pktstream_open(struct inode *node, struct file *file_p){
	minor_file * current_minor;
	
	// retrieving minor number from file descriptor
	int minor = iminor(file_p -> f_path.dentry -> d_inode);
	printk(KERN_INFO "%s: opening minor number %d\n", DEVICE_NAME, minor);

	// check if minor number is valid
	if (minor > 255) {
		printk(KERN_ALERT "%s: warning opening an invalid minor number %d\n", DEVICE_NAME, minor);
		return -1;
	}	

	// if minor number data structure is not initialized, do it
	if (minor_files[minor] == NULL) {
		current_minor = kmalloc(sizeof(minor_file), GFP_KERNEL);
		if (!current_minor) {
			printk(KERN_ALERT "%s: could not allocate memory for current minor %d\n", DEVICE_NAME, minor);
			return -1;
		}
		
		current_minor -> clients = 1;
		current_minor -> data_count = 0;
		current_minor -> first_segment = NULL;
		current_minor -> last_segment = NULL;
		printk(KERN_INFO "%s: initialized structures for minor number %d\n", DEVICE_NAME, minor);
		minor_files[minor] = current_minor;
	} else {
	// update the connected clients counter for the current minor
		current_minor = minor_files[minor];
		current_minor -> clients++;
		printk(KERN_INFO "%s: update client count %d for minor number %d\n", DEVICE_NAME, current_minor -> clients, minor);
	}

	return 0;
}

int pktstream_release(struct inode *node, struct file *file_p){
	minor_file * current_minor;
	int minor;	

	minor = retrieve_minor_number(file_p, "release");
	if (minor == -1) return -1;

	// decrease clients counter for minor file
	current_minor = minor_files[minor];
	current_minor -> clients--;
	printk(KERN_INFO "%s: update client count %d for minor number %d\n", DEVICE_NAME, current_minor -> clients, minor);

	// if no clients are connected, release the data structure
	if (current_minor -> clients == 0) {
		kfree(current_minor);
		minor_files[minor] = NULL;
		printk(KERN_INFO "%s: freed data structures related to minor number %d\n", DEVICE_NAME, minor);
	}

	return 0;
}



/*
 * Module read and write
 */

ssize_t pktstream_read(struct file *file_p, char *buff, size_t count, loff_t *f_pos){
	minor_file * current_minor;
	segment * current_segment;
	int minor;
	
	minor = retrieve_minor_number(file_p, "read");
	if (minor == -1) return -1;
	current_minor = minor_files[minor];

	if (current_minor -> first_segment == NULL){
		printk(KERN_ALERT "%s: warning reading empty minor %d\n", DEVICE_NAME, minor);
		return -1;
	}

	/* if operative mode is PACKET it must read a single packet;
	 * any bytes not fitting must be discarded
	 *
	 * if operative mode is STREAM it must read packets until receiving buffer
	 * is filled; residual bytes will become a new packet
	 */
	if (op_mode == PACKET) {
		current_segment = current_minor -> first_segment;
		current_minor -> first_segment = current_segment -> next;		
		if (current_segment -> segment_buffer != NULL) {
			print_bytes(current_segment -> segment_buffer, current_segment -> segment_size); 
		}

	}

	/*copy_to_user(buff, pktstream_buffer, 1);
	if (*f_pos == 0){
		*f_pos += 1;
		return 1;
	} else {
		return 0;
	}*/
	return 1;
}	

ssize_t pktstream_write(struct file *file_p, char *buff, size_t count, loff_t *f_pos) {
	minor_file * current_minor;
	byte *tmp;
	unsigned int num_pkts;
	unsigned int residual_bytes;
	int minor;
	int i;
		
	minor = retrieve_minor_number(file_p, "write");
	if (minor == -1) return -1;
	current_minor = minor_files[minor];

	// check size of write is admissible
	if ((count <= 0) || (current_minor -> data_count + count) >= MAX_FILE_SIZE) {
		printk(KERN_ALERT "%s: warning message size not admissible %zd\n", DEVICE_NAME, count);
		return -1;
	}	

	// compute number of packets necessary to contain data
	num_pkts = count / pkt_size;
       	residual_bytes = count % pkt_size;	

	// generates the list of new segments
	for (i = 0; i < num_pkts; i++) {
		tmp = buff + (pkt_size * i);
		create_append_segments(current_minor, pkt_size, tmp);
	}
	if (residual_bytes != 0) {
		tmp = buff;
		create_append_segments(current_minor, residual_bytes, tmp);
	}

 	/*tmp = buff + count - 1;
 	copy_from_user(pktstream_buffer, tmp, 1);*/
	return 1; 
}	




/*
 * Helper functions
 */

int retrieve_minor_number(struct file *file_p, char * operation) {
	int minor;
	
	// retrieving minor number from file descriptor
	minor = iminor(file_p -> f_path.dentry -> d_inode);
	printk(KERN_INFO "%s: %s on minor number %d\n", DEVICE_NAME, operation, minor);

	// check if minor number is valid
	if (minor > 255) {
		printk(KERN_ALERT "%s: %s on invalid minor number %d\n", DEVICE_NAME, operation, minor);
		return -1;
	}

	// check if minor number was not previously initialized
	if (minor_files[minor] == NULL) {			
		printk(KERN_ALERT "%s: %s on non initialized minor number %d\n", DEVICE_NAME, operation, minor);
		return -1;
	}

	return minor;
}

void create_append_segments(minor_file * current_minor, unsigned int cur_size, byte * tmp) {
	segment * current_segment;

	// allocate new segment with buffer of specified size
	current_segment = kmalloc(sizeof(segment), GFP_KERNEL);
	current_segment -> segment_size = cur_size;
	current_segment -> next = NULL;
	current_segment -> segment_buffer = kmalloc(cur_size, GFP_KERNEL);
	copy_from_user(current_segment -> segment_buffer, tmp, cur_size);

	// check if the minor file list is empty
	if (current_minor -> last_segment == NULL) {
		current_minor -> first_segment = current_segment;
		current_minor -> last_segment = current_segment;
	} else {
		// otherwise add segment to the end of the list
		current_minor -> last_segment -> next = current_segment;
		current_minor -> last_segment = current_segment;
	}

	//print_bytes(current_segment -> segment_buffer, current_segment -> segment_size);
	print_bytes(tmp, cur_size);
	current_minor -> data_count += cur_size;
}

void print_bytes(byte * buff, unsigned int cur_size) {
	int i;

	printk(KERN_INFO "%s: %d bytes ", DEVICE_NAME, cur_size); 
	for (i = 0; i < cur_size; i++)
		printk(KERN_INFO "%02X ", buff[i]);
	printk(KERN_INFO "\n");
}

