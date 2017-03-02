obj-m += pktstream.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules 
	sudo mknod devfile c 75 0 -m 666
	sudo mknod devfile1 c 75 1 -m 666
	sudo insmod pktstream.ko
	gcc -o test test.c

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	sudo rmmod pktstream
	rm devfile
	rm devfile1
	rm test 
	sudo dmesg --clear
