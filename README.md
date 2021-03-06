# Multi Mode Packet/Stream Device Driver

This project contains the code for a multi-mode device driver for the Linux
Kernel.  Goal of the driver is to provide a device file adherent to the
following, provided, specifications.


### Specifications

This specification relates to the implementation of a special device file that
is accessible according to FIFO style semantic (via open/close/read/write
services). Any data segment that is posted to the stream associated with the
file is seen either as an independent data unit (a packet) or a portion of a
unique stream.

An ioctl command must be supported to specify whether the current operating
mode of the device file is stream or packet. This only affects read operations,
since write operations simply post data onto the device file unique logical
stream. Packets, say stream segments, currently buffered within the device file
are delivered upon read operations according to a streaming (TCP-like) rule if
the stream operating mode is posted via the ioctl command. Otherwise they are
delivered just like individual packets. Packet delivery must lead to discard
portions of a data segment in case the requested amount of bytes to be
delivered is smaller that the actual segment size. On the other hand, the
residual of undelivered bytes must not be discarded if the current operating
mode of the device file is stream. Such residual will figure out, in its turn,
as an independent data segment.

The device file needs to be multi-instance (by having the possibility to manage
at least 256 different instances) so that multiple FIFO style streams
(characterized by the above semantic) can be concurrently accessed by active
processes/threads. File system nodes associated with the same minor number need
to be allowed, which can be managed concurrently.

The device file needs to also support other ioctl commands in order to define
the run time behavior of any I/O session targeting it (such as whether read
and/or write operations on a session need to be performed according to blocking
or non-blocking rules).

Parameters that are left to the designer, which should be selected via
reasonable policies, are:

 - the maximum size of device file buffered data or individual data segments
   (this might also be made tunable via ioctl up to an absolute upper limit)
 - the maximum storage that can be (dynamically) reserved for any individual
   instance of the device file
 - the range of device file minor numbers supported by the driver (it could be
   the interval [0-255] or not) 


### Design

The module was designed considering no a-priori assumption on the usage model
(such as frequent short messages or sporadic long messages). Concurrency is
granted through the use of mutex objects and a read and  write queues are
provided for each file, to allow blocking and non-blocking access modes.

Each file is organized as a linked list of data segments. The main data
structures employed are:
 
 - minor_file
    - current number of connected clients and amount of data contained
    - current data segment size and maximum file size for writing operations
    - read/write access mutex
    - read and write wait queues
    - current access and operational modes (blocking/non-blocking, packet/stream)
    - pointers to the first and last segments of the maintained linked list
 - segment 
    - segment length
    - pointer to next segment
    - pointer to the actual data buffer

The two segment pointers are used for fast access to data during read and write
operations given the FIFO semantic.


### Use

The module can be compiled with the provided make-file. The major number used
is 75. Besides the provided test script, the device file can be tested with 
standard shell tools like cat and echo.


