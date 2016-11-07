BSD LICENSE

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
Guest Server Scaling is a service to allow a guest to scale the capacity of a
single guest server up and down on demand.

Current supported scaling operation is CPU scaling.

The resources can be scaled up/down from the nova CLI or GUI.  Scaling can also
be set up via heat to be automatically triggered based on Ceilometer statistics.
(This will not be covered in this document, see the full documentation and the
heat SDK for how to configure heat templates for scaling a single guest server.)

This package contains an agent and a number of scripts to be included in the
guest image.  These will handle the guest side of the coordinated efforts
involved in scaling up/down guest resources.


DEPENDENCIES
============
    NOTE that this wrs-guest-scale SDK module has both a compile-time and run-time
    dependency on the wrs-server-group SDK module.

    This wrs-guest-scale SDK module requires that the wrs-server-group SDK tarball 
    has been previously extracted and built, and that the resulting libraries 
    and headers have been placed in a location that can be found by the normal build 
    tools, or the WRS_SERVER_GROUP_DIR environment variable has been set.

    The output of BOTH the wrs-guest-scale SDK module and the wrs-server-group SDK
    module are required to be installed in a guest image for guest resource scaling.


REQUIREMENTS
============
    Compilation:
        Linux OS, x86_64 architecture
        gcc compiler
        development libraries and headers for glibc
        development libraries and headers for libguesthostmsg
            (built by the wrs-server-group SDK package)
        development libraries and headers for json-c

    VM Runtime:
        Linux OS, x86_64 architecture; CONFIG_HOTPLUG_CPU=y|m
        runtime libraries for glibc
        runtime libraries for libguesthostmsg
        "guest_agent" binary daemon
            (provided by the wrs-server-group SDK package)
        runtime libraries for json-c

    The code has been tested with glibc 2.15, gcc 4.6 and json-c 0.12.99 but it
    should run on other versions without difficulty.


DELIVERABLE
===========
The Guest Server Scaling service is delivered as source with the required
Makefiles in a compressed tarball called "wrs-guest-scale-#.#.#.tgz", such that
it can be compiled for the applicable guest linux distribution.


COMPILE
=======
Pre-requisite:
    Ensure that the wrs-server-group SDK tarball has been previously extracted and
    built, and that the resulting libraries and headers have been placed in a
    location that can be found by the normal build tools, or the WRS_SERVER_GROUP_DIR
    environment variable has been set.

Extract the tarball contents:

    tar xvf wrs-guest-scale-#.#.#.tgz

To compile:

    # Note: assumes wrs-server-group-#.#.#.tgz has already been extracted and compiled.

    cd wrs-guest-scale-#.#.#

    # If wrs-guest-scale-#.#.#.tgz and wrs-server-group-#.#.#.tgz where extracted in a common directory
    make

    # Otherwise supply the path to where wrs-server-group can be found.  e.g.
    make WRS_SERVER_GROUP_DIR=/usr/src/wrs-server-group-#.#.#

This will produce:

1) An executable "bin/guest_scale_agent".  This handles the basic vCPU scaling
in the guest, and calls out to a helper script if present to support
application-specific customization.  It should be configured to respawn
(via /etc/inittab or some other process monitor) if it dies for any reason.
This executable must be installed into the guest (e.g. in "/usr/sbin") and configured 
to run at startup as early as possible.
NOTE
   The "guest_agent" executable from the wrs-server-group SDK package MUST ALSO
   be installed into the guest (e.g. in "/usr/bin"), configured to run at startup 
   as early as possible and configured to respawn via /etc/inittab or some other 
   process monitor (in case it dies for any reason).

2) A script "script/app_scale_helper".  This is an optional script that is
intended to allow for app-specific customization.  If present, it must be
installed in "/usr/sbin".  If present, it will be called by "guest_scale_agent"
when scaling in either direction.

3) A script "script/offline_cpus".  This must be run later in the init sequence,
after guest_scale_agent has started up but before the application has started
any CPU-affined applications.  A helper script "script/init_offline_cpus" has
been provided for systems using sysvinit.   The "offline_cpus" script will
offline vCPUs in the guest to match the status on the hypervisor.  This covers
the case where we are booting up with some CPUs offlined by the hypervisor.

Note:
The inclusion of the files into the build system and the guest image and the
configuration of the guest startup scripts is left up to the user to allow for
different build systems and init subsystems in the guest.


USAGE
=====
The service is designed to be simple to use.  A basic description is given
below, but more details are provided in the source and scripts.

1) Create a new flavor (or edit an existing flavor) such that the number of
vCPUs in the flavor matches the desired maximum number of vCPUs.  To specify the
minimum number of vCPUs, create an "extra spec" metadata entry for the flavor
with a key of "wrs:min_vcpus" and a value that is an integer number between one
and the max number of vCPUs.  This can be done from the CLI or the GUI.  (In the
GUI select the "Admin" tab, go to the "Flavor" navigation link, click on a
flavor name, select the "Extra Specs" tab, click on "Create", select
"Minimum Number of CPUs" from the pulldown, and enter the desired value.)

2) Build BOTH the wrs-server-group SDK package and this wrs-guest-scale package,
and install the output of BOTH packages in an image.  Lastly, ensure that the 
CONFIG_HOTPLUG_CPU kernel config option is set in the image kernel.

3) Boot the image.  It will come up with the full set of vCPUs.

4) To reduce the number of online vCPUs in the guest server, run
"nova scale <server> cpu down" from the controller (or anywhere else you can run
nova commands).  This will pass a message up into the guest, where it will be
handled by "guest_scale_agent".  That in turn will call out to
"/usr/sbin/app_scale_helper" (if it exists) which is expected to pick a vCPU to
offline.  This script can be modified/replaced as needed for application-
specific purposes.  By default, it will select the highest-numbered online vCPU.
If the script isn't present or errors out, then "guest_scale_agent" will itself 
select the highest-numbered online vCPU as the one to be offlined.  It will then
tell the guest kernel to offline the selected vCPU, and will pass the selected
vCPU back down to the hypervisor, which will adjust vCPU affinity so that the
underlying physical CPU can be freed up for use by other VMs.  At this point
displaying the information for the guest server will show it using less than the
maximum number of cpus.

5) To increase the number of online vCPUs, run "nova scale <server> cpu up".
Assuming the resources are available the hypervisor will allocate a physical CPU
and will associate it with the guest server.  "guest_scale_agent" will set the
lowest-numbered offline vCPU to "online", and will pass the vCPU number to
"/usr/sbin/app_scale_helper" (if it exists) for the application to do any
special handling that may be required.


The behaviour of a scaled-down server during various nova operations is as
follows:

live migration: server remains scaled-down
pause/unpause: server remains scaled-down
stop/start: server remains scaled-down
evacuation: server remains scaled-down
rebuild: server remains scaled-down
automatic restart on crash: server remains scaled-down
cold migration: server reverts to max vcpus
resize: server reverts to max vcpus for the new flavor

If a snapshot is taken of a scaled-down server, a new server booting the
snapshot will start with the number of vCPUs specified by the flavor.

CAVEATS
=======
It is possible for the scale-up operation to fail if the compute node has
already allocated all of its resources to other guests.  If this happens,
the system will not do any automatic migration to try to free up resources.
Manual action will be required to free up resources.

Any CPUs that are handling userspace DPDK/AVP packet processing should not be
offlined.  It may appear to work, but may lead to packet loss.  This can be
enforced by setting the wrs:min_vcpus value appropriately high.

If hyperthreading is used, the flavor must set hw:cpu_threads_policy to
isolate and set cpu_policy to dedicated. 
