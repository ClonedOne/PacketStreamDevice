obj-m += pktstream.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules 
	sudo mknod devfile c 75 0 -m 666
	sudo mknod devfile1 c 75 1 -m 666
	sudo insmod pktstream.ko

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	sudo rmmod pktstream
	sudo dmesg --clear
	rm devfile
	rm devfile1
	rm test 

test:
	gcc -c pktstream_lib.c
	gcc -c test.c
	gcc -o test test.o pktstream_lib.o -lpthread 

