obj-m += pktstream.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules 
	sudo mknod devfile c 75 0 -m 666
	sudo insmod pktstream.ko

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm devfile
	sudo rmmod pktstream

