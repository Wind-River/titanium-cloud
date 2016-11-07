
Copyright(c) 2013-2016, Wind River Systems, Inc. 

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in
    the documentation and/or other materials provided with the
    distribution.
  * Neither the name of Wind River Systems nor the names of its
    contributors may be used to endorse or promote products derived
    from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
-----------------------------------------------------------------------

DESCRIPTION
===========

Server Group Messaging is a service to provide simple low-bandwidth datagram
messaging and notifications for servers that are part of the same server group.
This messaging channel is available regardless of whether IP networking is
functional within the server, and it requires no knowledge within the server
about the other members of the group.

The service provides three types of messaging:
1) Broadcast: this allows a server to send a datagram (size of up to 3050 bytes)
   to all other servers within the server group.

2) Notification: this provides servers with information about changes to the
   state of other servers within the server group.

3) Status: this allows a server to query the current state of all servers within
   the server group (including itself).

This service is not intended for high bandwidth or low-latency operations.  It
is best-effort, not reliable.  Applications should do end-to-end acks and
retries if they care about reliability.


REQUIREMENTS
============
    Compilation:
        Linux OS, x86_64 architecture
        gcc compiler
        development libraries and headers for glibc
        development libraries and headers for json-c

    VM Runtime:
        Linux OS, x86_64 architecture
        runtime libraries for glibc
        runtime libraries for json-c

    The code has been tested with glibc 2.15, gcc 4.6 and json-c 0.12.99 but it
    should run on other versions without difficulty.


DELIVERABLE
===========
The Server Group Messaging service is delivered as source with the
required makefiles in a compressed tarball called
"wrs-server-group-2.0.0.tgz", such that it can be compiled for the applicable
guest linux distribution.


COMPILE
=======
Extract the tarball contents:

    tar xvf wrs-server-group-#.#.#.tgz

To compile:

    cd wrs-server-group-#.#.#
    make
    WRS_SERVER_GROUP_DIR=${PWD}

This will produce:

1) An executable "bin/guest_agent".  This acts as a relay between the guest and
the host.  This executable must be installed into the guest and configured to
run at startup.  It should be configured to respawn (via /etc/inittab or some
other process monitor) if it dies for any reason.

2) A library "lib/libservergroup.so.2.0.0".  This encapsulates the details of
talking to the guest agent.  This can be used with the header file
"server_group.h" to create custom applications that can make use of the
messaging service.  This should be installed within the guest in a suitable
library location such that the linker can find it.  It should also be installed
in the build system where custom applications can link against it.  The
"libservergroup.so.2" symlink should be created in the build system and the
guest, and the "libservergroup.so" symlink should be created in the build
system, either by ldconfig or some other means.

3) A header file "include/cgcs/server_group.h".  This is used along with the library to
create custom applications that can make use of the messaging service.  This
should be installed in the build system where custom applications can include
it.

4) A sample program "bin/server_group_app" is created in order to show the use
of the APIs and validate that they are working properly in the guest.

5) A library "lib/libguesthostmsg.so.2.0.0".  This library is used by
wrs-guest-scale component packaged separately.  This can be used with the
header file "guest_host_msg.h".  This should be installed within
the guest in a suitable library location such that the linker can find it.  
It should also be installed in the build system where custom applications can 
link against it.  The "libguesthostmsg.so.2" symlink should be created in the
build system and the guest, and the "libguesthostmsg.so" symlink should be 
created in the build system, either by ldconfig or some other means.

6) A header file "include/cgcs/guest_host_msg.h".  This is used along with 
the library to create custom applications that can make use of the messaging 
service.  This should be installed in the build system where custom
applications can include it.


Note:
The inclusion of the files into the build system and the guest image and the
configuration of the guest startup scripts is left up to the user to allow for
different build systems and init subsystems in the guest.


INSTALL
=======
Installing in a running VM:

    As the root user
    1) Copy "bin/guest_agent" and any other desired binaries such as
       "bin/server_group_app" to /usr/sbin in the VM.
   
    2) Copy "lib/libservergroup.so.2.0.0" and "lib/libguesthostmsg.so.2.0.0"
       to /usr/lib64 in the VM.
    
    3) Run "ldconfig".  If ldconfig is unavailable, then subststute
         cd /usr/lib64
         ln -s libservergroup.so.2.0.0 libservergroup.so.2
         ln -s libservergroup.so.2 libservergroup.so
         ln -s libguesthostmsg.so.2.0.0 libguesthostmsg.so.2
         ln -s libguesthostmsg.so.2 libguesthostmsg.so
    
    4) Run "/usr/sbin/guest_agent" in the background.
    
    5) Applications can now make use of the service.

    6) cp -r include/cgcs /usr/include/

As part of building the VM ISO Image:

    1) Ensure "bin/guest_agent" and any other desired binaries get installed
       to /usr/sbin in the VM filesystem.

    2) Ensure "lib/libservergroup.so.2.0.0" and "lib/libguesthostmsg.so.2.0.0"
       gets installed to /usr/lib64 in the VM filesystem.

    3) Ensure that "ldconfig" will run at VM startup or else create the
       following symlinks in the "/usr/lib64" directory of the VM filesystem.
         cd ${VM_ROOT}/usr/lib64
         ln -s libservergroup.so.2.0.0 libservergroup.so.2
         ln -s libservergroup.so.2 libservergroup.so
         ln -s libguesthostmsg.so.2.0.0 libguesthostmsg.so.2
         ln -s libguesthostmsg.so.2 libguesthostmsg.so

    4) Ensure that "/usr/sbin/guest_agent" is configured to run automatically
       at VM startup and to respawn if it dies for any reason.


USAGE
=====
The service is designed to be simple to use.  A basic description is given
below, but more details are provided in the header file.

First, the application must call init_sg().  This call takes three function
pointers corresponding to callbacks for the various message types.  If an
application doesn't intend to use one or more of the message types then the
appropriate function pointers can be specified as NULL.  The function returns a
socket that must be monitored for activity using select/poll/etc.

When the socket becomes readable, the application must call process_sg_msg().
This may result in callbacks being called so be careful about deadlock.

In order to send a broadcast message to every server in the server group, the
application can call sg_msg_broadcast().

In order to request the status of all servers in the group, the application can
call sg_request_status().

The sg_broadcast_msg_handler_t() callback will be called when receiving a server
group broadcast message.  It will be passed the message as well as a
null-terminated string containing the instance name of the sender.

The sg_notification_msg_handler_t() callback will be called when receiving a
notification message indicating a status change in one of the members of the
server group.  The msg is JSON-formatted, essentially the normal notification
that gets sent out by OpenStack's notification service, but with some 
non-relevant information removed to keep the message shorter.

The sg_status_msg_handler_t() callback will be called when receiving the
response to a status query.  The message is a JSON-formatted list of
dictionaries, each of which represents a single server.
    

SAMPLE APPLICATION
==================
The "server_group_app" sample application can be used to test the various types
of messages.  It takes one optional argument, consisting of a character string
in quotes.  When run, it behaves as follows:

1) It registers for all three callbacks.
2) If the optional argument was specified it sends that string as a server group
   broadcast message.
3) It requests the group status.
4) It then goes into a loop waiting for incoming messages.  Any incoming message
   will be printed out along with information about the type of message.
   
   
The service can be validated as follows using the "server_group_app" sample
application:
a) Create a server group with the "affinity" scheduler policy.
b) Start up a server within the server group.
c) Run "server_group_app" in the first server.  You should immediately see the
   status response message containing information about that server.
d) Start up a second server within the server group.  You should see
   notification messages being received in the first server.
e) Run 'server_group_app "this is a test"' on the second server.  You should see
   "this is a test" received as a broadcast message on the first server. The
   second server should show a status response with information about both
   servers
f) Start up a third server within the server group.  You should see
   notification messages being received in the other two servers.
g) Run 'server_group_app "final test"' on the second server.  You should see
   "final test" received as a broadcast message on the other two servers. The
   second server should show a status response with information about all three
   servers
