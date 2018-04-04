# Titanium-Server

Network Driver source code and Guest API SDK Modules for the Wind River Titanium Server Platform.

Contains both Titanium Server R2 / 15.12 Version of Network Drivers and Guest API SDK Modules, AND
              Titanium Server R3 / 16.10 Version of Network Drivers and Guest API SDK Modules.

The AVP kernel module is now stored at the following github repository.

    https://github.com/Wind-River/titanium-cloud-avp-kmod


The AVP DPDK PMD is now a part of the DPDK source library.  We strongly recommend using a version
of the DPDK that includes the AVP PMD (i.e., v17.05+).  If an older version of the DPDK is required
we provide an out of tree version of the driver in this repo.  This archived version will eventually
be discontinued.  The latest DPDK release can be found at the following link.

    https://dpdk.org

