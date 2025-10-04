# MTU1280d with User-mode Linux
This is an alternative to simulating 1280 octet PMTU without packet interception
using Netfilter and the application("mtu1280d") generating ICMPv6 type 2(packet
too big) messages using user-mode Linux(UML).

This set up is only useful if the machine has no modern kernel. If a modern
kernel is available, [the netns](/netns/README.md) set up is more favourable as
it requires less memory and less overhead.

In this set up, the ICMPv6 messages will originate from a middlebox, which is
the host running the UML with the proper source address and hop limit. This is
more in line with the real-world scenario.

[RFC4443](https://datatracker.ietf.org/doc/html/rfc4443#section-3.2):

>   Originating a Packet Too Big Message makes an exception to one of the
>   rules as to when to originate an ICMPv6 error message.  Unlike other
>   messages, it is sent in response to a packet received with an IPv6
>   multicast destination address, or with a link-layer multicast or
>   link-layer broadcast address.

## Network Set Up
If the CSP supports prefix delegation to an instance, get that. AWS and GCP
supports this. Hetzner provides IPv6 connectivity to the VMs through prefix
delegation by default(although there's no DHCP support). If prefix delegation is
not available, assign multiple addresses to the instance. The point here is to
get the hypervisor to route the packets to the instance.

Pick an address for the host which will act as the middlebox before the 1280 mtu
segment so that it can send ICMP packets. Pick another address for the UML.

## INSTALL
Here are the packages required to build and run the UML kernel and the rootfs
image. radvd is used to set the default route on the UML system so we don't have
to manually set the link-local address of the host tap device, which could
change.

```sh
dnf install xz gzip tar bc e2fsprogs wget git gcc make perl flex bison
# optional - to set the default route using RA
dnf install radvd
```

Create the tap device for the UML process to tap into. Replace `IPADDR_FROM_CSP`
with the one you picked for the inner UML host earlier. The IPv4 connectivity is
optional so the line can be deleted.

```sh
nmcli c add \
	type tun \
	con-name tap-mtu1280 \
	ifname tap-mtu1280 \
	autoconnect yes \
	tun.mode tap \
	tun.vnet-hdr yes \
	ipv4.method disabled \
	ipv6.method link-local \
	ipv4.routes "IPADDR_FROM_CSP/32 mtu=1280" \
	ipv6.routes "IPADDR_FROM_CSP/128 mtu=1280"
```

There are two ways to set the IP addresses of the UML interface. You can either
edit the [interfaces file](/um/target/interfaces) before building the rootfs
image or edit it in the UML after building it. Do not set the gateway for IPv6
as the kernel will automatically set one from the RA from the host.

In the um directory of the project, do:

```sh
make
sudo make rootimg
sudo make install

# if using radvd
sudo make install-radvd
sudo systemctl enable --now radvd.service
```

Edit the uml launch parameters in `/etc/mtu1280/mtu1280-uml.env` to set the
mem to the proper size for the system.

Now the UML kernel program and the root image are installed on the system. To
run the UML to edit the interfaces file or just to play with it, run:

```sh
sudo MTU1280_UML_OPTS_EXTRA=init=/bin/sh make run-uml
```

If the UML process crashes trying to mmap() temp files, change the SELinux
policy:

```sh
setsebool -P selinuxuser_execmod 1
```

After making changes, don't forget to do `sync` or `umount /` before crashing
the kernel by exiting from the shell.

You can test the UML image by letting it boot normally. The
[mtu1280-init.sh](/um/target/mtu1280/mtu1280-init.sh) script will compile and
install mod_ip. If everything goes well, you will fall through to the root
shell(getty disabled for convenience) and mod_ip will be running with the
apache2 service.

If you're sure that the UML will boot and function on its own, run it as a
service.

```sh
systemctl enable --now mtu1280-uml.service
```

## Route Check
```
 13.|-- 2001:db8:4000:cfff::f200:  0.0%     4   81.9  42.3  29.0  81.9  26.4
 14.|-- 2001:db8:f0:f310::1500    0.0%     4   30.1  30.1  30.1  30.1   0.0
 15.|-- 2001:db8:f0:f310::1280    0.0%     4   30.2  31.1  30.2  33.7   1.8
```

`traceroute`ing the UML host should give you something like the above(1500 the
host and 1280 the UML).

https://tools.keycdn.com/traceroute

## Caveats and Quirks
The script [mtu1280-hook](/um/host/dispatcher.d/mtu1280-hook) is installed on
the system to disable some offloading features of the interface. The script is
made mainly because the UML network stack cannot handle GRO frames gracefully.
The UML process becomes unstable when it receives a GRO frame routed from the
host interface. This will have negligible network performance impact as mod_ip
is not traffic-heavy, provided that it's the only service the host runs.

The UML is not so stable. Here are few more problems with it:

- Rebooting will make it unusable. Always power off or kill the process by
  stopping the service
- The UML is more of a kernel debugging tool than a full-blown virtualization
  solution. The performance and stability can become an issue
- The UML provides no virtual no serial device. If you don't have the main
  serial through the stdio of the process(which is the case when it's running as
  a Systemd service), there are other ways to get the shell:
  - SSH: set the root password, enable and start dropbear
  - Use PTY or TTY option of the UML

Be very careful if you decide to use Dropbear on the UML. The firewall of most
CSPs support destination address in the rule set. This means that filtering must
be done on either the host or UML.

### One Way MTU1280
```
ROUTER A -------------------- ROUTER B -------------------- mod_ip HOST
           MTU 1280 segment              MTU 1500 segment
```

The full-blown set up requires 3 hosts as illustrated above. As this set up is
designed to be run on a cloud instance, corners had to be cut by making the
"imaginary" MTU 1280 segment using the routing table on the host.

```
                 HOST                               UML
<- default route   |   tap device ---->    <---- default route
     MTU: 1500     |    MTU: 1280                 MTU: 1500
```

So the UML can still send packets larger than 1500 octets whilst the host cannot
route packets larger than 1280 octets addressed to the UML due to the routing
constraint. Setting the MTU of the UML interface to 1280 will make it announce
the MSS of 1240 bytes in the SYN packets. Modifying the value using Netfilter
requires kernel patch because there's a safety check that disallows values
larger than that of the device.

In function `tcpmss_mangle_packet()` in net/netfilter/xt_TCPMSS.c:

```c
			/* Never increase MSS, even when setting it, as
			 * doing so results in problems for hosts that rely
			 * on MSS being set correctly.
			 */
			if (oldmss <= newmss)
				return 0;
```

Even with this check removed, the TCPMSS module is unable to modify the MSS
value to the one requested precisely. So we're left with the routing table MTU
approach.

Some IP stack implementations could get confused or complain. However, all the
endpoint has to do is cache the new MTU and adjust the MSS of the established
connection before performing retransmission.
