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
 * Global variables for the module
 */

byte * pktstream_buffer;


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
	// pointer to the first data segment in the minor file
	struct segment * first_segment;

	// pointer to the last data segment in the minor file
	struct segment * last_segment;
} minor_file;



/*
 * Function declarations
 */

int pktstream_open(struct inode *node, struct file *file_p);

int pktstream_release(struct inode *node, struct file *file_p);

ssize_t pktstream_read(struct file *file_p, char *buff, size_t count, loff_t *f_pos);

ssize_t pktstream_write(struct file *file_p, char *buff, size_t count, loff_t *f_pos);

void pktstream_exit(void);

int pktstream_init(void);


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
 * Function implementations
 */

int pktstream_init(void) {
	int major_num;
	
	// Treis to register device major number
	major_num = register_chrdev(MAJOR_NUM, DEVICE_NAME, &pktstream_fops); 
	if (major_num < 0){
		printk(KERN_ALERT "%s: cannot obtain major number %d\n", DEVICE_NAME, MAJOR_NUM);
		return major_num;
	}
	printk(KERN_INFO "%s: registered correctly with major number %d\n",DEVICE_NAME, MAJOR_NUM);	

	// Tries to allocate memory for the buffer
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

int pktstream_open(struct inode *node, struct file *file_p){
	return 0;
}

int pktstream_release(struct inode *node, struct file *file_p){
	return 0;
}

ssize_t pktstream_read(struct file *file_p, char *buff, size_t count, loff_t *f_pos){
	copy_to_user(buff, pktstream_buffer, 1);
	if (*f_pos == 0){
		*f_pos += 1;
		return 1;
	} else {
		return 0;
	}
}	

ssize_t pktstream_write(struct file *file_p, char *buff, size_t count, loff_t *f_pos) {
	char *tmp;
	tmp = buff + count - 1;
	copy_from_user(pktstream_buffer, tmp, 1);
	return 1; 
}	


