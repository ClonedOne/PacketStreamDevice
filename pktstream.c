#define EXPORT_SYMTAB
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/sched.h>	
#include <linux/wait.h>	
#include <linux/pid.h>	
#include <linux/tty.h>
#include <linux/version.h>
#include <asm/mutex.h>
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
 * Global variables for the module
 */

// array of pointers to minor_file data structures
static minor_file * minor_files[256] = {NULL};

// general lock for global variable modifications
struct mutex general_lock;



/*
 * Function declarations
 */

int pktstream_open(struct inode *node, struct file *file_p);

int pktstream_release(struct inode *node, struct file *file_p);

ssize_t pktstream_read(struct file *file_p, char *buff, size_t count, loff_t *f_pos);

ssize_t pktstream_write(struct file *file_p, const char *buff, size_t count, loff_t *f_pos);

void pktstream_exit(void);

int pktstream_init(void);

long pktstream_ioctl(struct file *file_p, unsigned int ioctl_cmd, unsigned long ioctl_arg);

void create_append_segments(minor_file * current_minor, unsigned int cur_size, const byte * tmp);

int retrieve_minor_number(struct file *file_p, char * operation);

void print_bytes(byte * buff, unsigned int cur_size);

int acquire_lock(minor_file * current_minor, int minor);



/*
 * Struct declaring the file access functions
 */

struct file_operations pktstream_fops = {
	.read = pktstream_read,
	.write = pktstream_write,
	.open = pktstream_open,
	.release = pktstream_release,
	.unlocked_ioctl = pktstream_ioctl
};



/*
 * Mandatory initialization and exit functions
 */

module_init(pktstream_init);

module_exit(pktstream_exit);



/*
 * Module life-cycle utilities
 */

int pktstream_init(void) {
	int major_num;
	
	// Try to register device major number
	major_num = register_chrdev(MAJOR_NUM, DEVICE_NAME, &pktstream_fops); 
	if (major_num < 0){
		printk(KERN_ALERT "%s: cannot obtain major number %d\n", DEVICE_NAME, MAJOR_NUM);
		return major_num;
	}
	printk(KERN_INFO "%s: registered correctly with major number %d\n",DEVICE_NAME, MAJOR_NUM);	

	mutex_init(&general_lock);

	printk(KERN_INFO "inserting module: %s\n", DEVICE_NAME);

	return 0;
}

void pktstream_exit(void){
	unregister_chrdev(MAJOR_NUM, DEVICE_NAME);
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

	// obtain general lock 
	if (acquire_lock(NULL, DEVICE_GENERAL_LOCK) != 0) return -ERESTARTSYS;

	// if minor number data structure is not initialized, do it
	if (minor_files[minor] == NULL) {
		current_minor = kzalloc(sizeof(minor_file), GFP_KERNEL);
		if (!current_minor) {
			printk(KERN_ALERT "%s: could not allocate memory for current minor %d\n", DEVICE_NAME, minor);
			mutex_unlock(&general_lock);
			return -1;
		}
		
		// initialize current minor's default values 
		current_minor -> first_segment = NULL;
		current_minor -> last_segment = NULL;
		current_minor -> clients = 1;
		current_minor -> data_count = 0;
		current_minor -> def_segment_size = PKT_DEFAULT_SIZE;
		current_minor -> file_size  = FILE_DEFAULT_SIZE;
		current_minor -> op_mode = PACKET;

		// initialize semaphore and wait queues
		mutex_init(&(current_minor -> rw_access));
		init_waitqueue_head(&(current_minor -> read_queue));
		init_waitqueue_head(&(current_minor -> write_queue));

		printk(KERN_INFO "%s: initialized structures for minor number %d\n", DEVICE_NAME, minor);
		minor_files[minor] = current_minor;
	} else {
	// update the connected clients counter for the current minor
		current_minor = minor_files[minor];
		current_minor -> clients++;
		printk(KERN_INFO "%s: update client count %d for minor number %d\n", DEVICE_NAME, current_minor -> clients, minor);
	}

	mutex_unlock(&general_lock);
	return 0;
}

int pktstream_release(struct inode *node, struct file *file_p){
	minor_file * current_minor;
	int minor;	

	minor = retrieve_minor_number(file_p, "release");
	if (minor == -1) return -1;

	// obtain general lock 
	if (acquire_lock(NULL, DEVICE_GENERAL_LOCK) != 0) return -ERESTARTSYS;

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
		
	mutex_unlock(&general_lock);
	return 0;
}



/*
 * Module read and write
 */

ssize_t pktstream_read(struct file *file_p, char *buff, size_t count, loff_t *f_pos){
	minor_file * current_minor;
	segment * current_segment;
	byte * temporary_buffer;
	int minor;
	size_t to_read;
	size_t already_read;
	size_t remaining_bytes;

	minor = retrieve_minor_number(file_p, "read");
	if (minor == -1) return -1;
	current_minor = minor_files[minor];
	
	// acquire lock
	if(acquire_lock(current_minor, minor) != 0) return -ERESTARTSYS;

	// check if there is no data to read
	while (current_minor -> first_segment == NULL){
		mutex_unlock(&(current_minor -> rw_access));

		// if non-blocking exit with error
		if (current_minor -> ac_mode == NON_BLOCK) {
			printk(KERN_ALERT "%s: warning reading empty file %d\n", DEVICE_NAME, minor);
			return -EAGAIN;
		}
		
		// if blocking put the client process to sleep
		if (wait_event_interruptible(current_minor -> read_queue, current_minor -> first_segment != NULL)){
			printk(KERN_ALERT "%s: interrupted while waiting to read %d\n", DEVICE_NAME, minor);
			return -1;
		}
		if (acquire_lock(current_minor, minor) != 0) return -ERESTARTSYS;
	}

	printk(KERN_INFO "%s: wants to read %zd bytes\n", DEVICE_NAME, count);
	current_segment = current_minor -> first_segment;

	/* if operative mode is PACKET it must read a single packet;
	 * any bytes not fitting must be discarded
	 */
	if (current_minor -> op_mode == PACKET) {
		printk(KERN_INFO "%s: reading as packet\n", DEVICE_NAME);
		current_minor -> first_segment = current_segment -> next;
		to_read = count < current_segment -> segment_size ? count : current_segment -> segment_size;
		copy_to_user(buff, current_segment -> segment_buffer, to_read);
		kfree(current_segment -> segment_buffer);
		kfree(current_segment);
		mutex_unlock(&(current_minor -> rw_access));
		return to_read;
	}

	/*
	 * otherwise operative mode is STREAM, it must read packets until receiving buffer
	 * is filled; residual bytes will become a new packet
	 */
	printk(KERN_INFO "%s: reading as stream\n", DEVICE_NAME);
	// already_read keeps the current amount of bytes read 
	already_read = 0;

	while (already_read < count && current_segment -> segment_buffer != NULL) {
		printk(KERN_INFO "%s: current segment size = %zd\n", DEVICE_NAME, current_segment -> segment_size);
		
		/* if the size of data contained in this segment plus what has already
		 * been read fit in the receiving buffer, read it
		 *
		 * else compute the size that can fit in receiving buffer and update
		 * current first segment with the new remaining data
		 */
		if ((already_read + current_segment -> segment_size) <= count){
			printk(KERN_INFO "%s: can read whole segment\n", DEVICE_NAME);
			to_read = current_segment -> segment_size;
			copy_to_user(buff + already_read, current_segment -> segment_buffer, to_read);
			current_minor -> first_segment = current_segment -> next;
			kfree(current_segment -> segment_buffer);
			kfree(current_segment);	
		} else {
			printk(KERN_INFO "%s: must split segment\n", DEVICE_NAME);
			remaining_bytes = (already_read + current_segment -> segment_size) - count;
			printk(KERN_INFO "%s: remaining_bytes = %zd\n", DEVICE_NAME, remaining_bytes);
			to_read = current_segment -> segment_size - remaining_bytes;
			copy_to_user(buff + already_read, current_segment ->segment_buffer, to_read);
			temporary_buffer = kzalloc(remaining_bytes, GFP_KERNEL);
			memcpy(temporary_buffer, current_segment -> segment_buffer + to_read, remaining_bytes);	
			kfree(current_segment -> segment_buffer);
			current_segment -> segment_buffer = temporary_buffer;
			current_segment -> segment_size = remaining_bytes;
		}

		printk(KERN_INFO "%s: to_read = %zd\n", DEVICE_NAME, to_read);
		already_read += to_read;
		printk(KERN_INFO "%s: already_read = %zd, count = %zd\n", DEVICE_NAME, already_read, count);
		current_segment = current_minor -> first_segment;
	}

	//wake up writers
	wake_up_interruptible(&current_minor -> write_queue);

	mutex_unlock(&(current_minor -> rw_access));
	return already_read;

}	

ssize_t pktstream_write(struct file *file_p, const char *buff, size_t count, loff_t *f_pos) {
	minor_file * current_minor;
	const byte *tmp;
	size_t num_pkts;
	size_t residual_bytes;
	size_t pkt_size;
	int minor;
	int i;
		
	minor = retrieve_minor_number(file_p, "write");
	if (minor == -1) return -1;
	current_minor = minor_files[minor];
	
	// acquire lock
	if (acquire_lock(current_minor, minor) != 0) return -ERESTARTSYS;

	pkt_size = current_minor -> def_segment_size;

	// check size of write is admissible
	if ((count == 0) || (current_minor -> data_count + count) >= current_minor -> file_size) {
		printk(KERN_ALERT "%s: warning message size not admissible %zd\n", DEVICE_NAME, count);
		mutex_unlock(&(current_minor -> rw_access));
		return -1;
	}	

	// check if new data would not fit in current available space
	while (current_minor -> data_count + count > current_minor -> file_size) {
		mutex_unlock(&(current_minor -> rw_access));
		
		// if non-blocking exit with error
		if (current_minor -> ac_mode == NON_BLOCK) {
			printk(KERN_ALERT "%s: warning not enough space to write %zd\n", DEVICE_NAME, count);
			return -EAGAIN;
		}

		// if blocking put the client process to sleep
		if (wait_event_interruptible(current_minor -> write_queue, current_minor -> data_count + count <= current_minor -> file_size)){
			printk(KERN_ALERT "%s: interrupted while waiting to write on %d\n", DEVICE_NAME, minor);
			return -1;
		}
		if (acquire_lock(current_minor, minor) != 0) return -ERESTARTSYS;
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

	// wake up readers
	wake_up_interruptible(&current_minor -> read_queue);

	mutex_unlock(&(current_minor -> rw_access));
	return count; 
}	



/*
 * Helper functions
 */

/*
 * retrieve minor number from file pointer 
 * check if related device file is initialized
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

/* 
 * create and append a new segment
 */
void create_append_segments(minor_file * current_minor, unsigned int cur_size, const byte * tmp) {
	segment * current_segment;

	// allocate new segment with buffer of specified size
	current_segment = kzalloc(sizeof(segment), GFP_KERNEL);
	if (!current_segment) {
		printk(KERN_ALERT "%s: could not allocate memory for new segment\n", DEVICE_NAME);
		return;
	}
	current_segment -> segment_size = cur_size;
	current_segment -> next = NULL;
	
	// allocate new segment data
	current_segment -> segment_buffer = kzalloc(cur_size, GFP_KERNEL);
	if (!current_segment -> segment_buffer){
		printk(KERN_ALERT "%s: could not allocate memory for segment data\n", DEVICE_NAME);
		kfree(current_segment);
		return;
	}
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

	print_bytes(current_segment -> segment_buffer, current_segment -> segment_size);
	current_minor -> data_count += cur_size;
}

/*
 * print the byte content of a buffer
 */
void print_bytes(byte * buff, unsigned int cur_size) {
	int i;

	printk(KERN_INFO "%s: %d bytes ", DEVICE_NAME, cur_size); 
	for (i = 0; i < cur_size; i++)
		printk(KERN_INFO "%02X ", buff[i]);
	printk(KERN_INFO "\n");
}

/*
 * acquire the lock on specified minor file
 * if interrupted exit signaling interruption
 */
int acquire_lock(minor_file * current_minor, int minor) {
	if (minor == DEVICE_GENERAL_LOCK) {
		if (mutex_lock_interruptible(&general_lock) != 0){
			printk(KERN_ALERT "%s: could not hold general lock\n", DEVICE_NAME);
			return -ERESTARTSYS;
		}
	} else {
		if (mutex_lock_interruptible(&(current_minor -> rw_access)) != 0){
			printk(KERN_ALERT "%s: could not hold lock on minor %d\n", DEVICE_NAME, minor);
			return -ERESTARTSYS;
		}
	}
	return 0;
}



/*
 * IOCTL function
 */

long pktstream_ioctl(struct file *file_p, unsigned int ioctl_cmd, unsigned long ioctl_arg){
	minor_file * current_minor;
	int minor;

	// variables initialization
	minor = retrieve_minor_number(file_p, "ioctl");
	if (minor == -1) return -1;
	current_minor = minor_files[minor];
	
	// acquire lock
	if (acquire_lock(current_minor, minor) != 0) return -ERESTARTSYS;

	switch(ioctl_cmd) {

	// set current operative mode to packet
	case PKTSTRM_IOCTL_SET_MODE_PACKET:
		current_minor -> op_mode = PACKET;
		break;

	// set current operative mode to stream
	case PKTSTRM_IOCTL_SET_MODE_STREAM:
		current_minor -> op_mode = STREAM;
		break;
	
	// set current access mode to blocking
	case PKTSTRM_IOCTL_SET_ACC_BLOCK:
		current_minor -> ac_mode = BLOCK;
		break;

	// set current access mode to non-blocking
	case PKTSTRM_IOCTL_SET_ACC_NO_BLOCK:
		current_minor -> ac_mode = NON_BLOCK;
		break;

	// set segment size to passed argument
	case PKTSTRM_IOCTL_SET_PKT_SIZE:
		if (ioctl_arg == 0 || ioctl_arg > MAX_PKT_SIZE) {
			mutex_unlock(&(current_minor -> rw_access));
			printk(KERN_ALERT "%s: ioctl invalid packet size %zd\n", DEVICE_NAME, ioctl_arg);
			return -1;
		}	
		current_minor -> def_segment_size = ioctl_arg;
		break;
	
	// set file size to passed argument
	case PKTSTRM_IOCTL_SET_FILE_SIZE:
		if (ioctl_arg == 0 || ioctl_arg > MAX_FILE_SIZE || ioctl_arg < current_minor -> data_count) {
			mutex_unlock(&(current_minor -> rw_access));
			printk(KERN_ALERT "%s: ioctl invalid file size %zd\n", DEVICE_NAME, ioctl_arg);
			return -1;
		}
		current_minor -> file_size = ioctl_arg;
		break;
	}
	
	mutex_unlock(&(current_minor -> rw_access));
	return 0;
}

