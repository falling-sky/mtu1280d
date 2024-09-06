# MTU1280 using Kernel Network Namespace
This set up is the "true" PMTU mismatch soft simulation using Linux kernel
netns. Each netns has its own virtual routing table and PMTU cache. The
bottleneck MTU segment is achieved through the MTU attribute of static route
entries.

```
                     ROUTER A         ROUTER B

<---------- mtu1280-a ------ mtu1280-b ------ mtu1280-c ---------->

to internet           |  MTU 1280 segment   |               to httpd
```

<pre>
$ tracepath -n fd12:34::3:1500:0
 1?: [LOCALHOST]                        0.025ms pmtu 1500
 1:  fd12:34::1:1280:0                                     0.128ms
 1:  fd12:34::1:1280:0                                     0.156ms
 2:  fd12:34::1:1280:0                                     0.244ms <b>pmtu 1280</b>
 2:  fd12:34::2:1280:0                                     0.235ms
 3:  fd12:34::3:1500:0                                     0.188ms reached
     Resume: pmtu 1280 hops 3 back 3
$ tracepath -n 10.12.80.34
 1?: [LOCALHOST]                      pmtu 1500
 1:  10.12.80.2                                            0.138ms
 1:  10.12.80.2                                            0.042ms
 2:  10.12.80.2                                            0.045ms <b>pmtu 1280</b>
 2:  10.12.80.18                                           0.055ms
 3:  10.12.80.34                                           0.051ms reached
     Resume: pmtu 1280 hops 3 back 3
</pre>

3 netns are create to hide the MTU 1280 links from both internet facing gateway
and httpd. This forces httpd to announce its MSS as 1440 octets and PMTUD is
carried out for each endpoint from the internet. The PMTU caching in
**mtu1280-c** is disabled so that the test clients can get the same result at
all times. The makefile recipe installs a "drop-in" service unit config to
override `NetworkNamespacePath` so httpd is run in the mtu1280-c netns.

The set up should perfectly simulate the kernel's behaviour. A few down sides
over the original mtu1280d approach are

1. more overhead as all 3 netns along the path need to be "simulated" -
   routing table, neighbor cache, PMTU cache ...
1. somewhat difficult to integrate netns to most programs as they're not
   designed with namespace in mind
1. complexity making it difficult to troubleshoot

## IPv4
The script was originally written to test the PMTUD of IPv6, but the IPv4
support has been added to test the PMTUD in IPv4 networks as well. As most
operating system does not do IPv4 PMTUD by default, the use of special system
API is required to set the DF flag. It's useful when any middlebox of the ISP
blocks `Fragmentation Needed` or fragmented packets in general.

## Network Set Up
See [/um/README.md#network-set-up](/um/README.md#network-set-up).

## INSTALL
Copy and edit the env file [mtu1280-netns.env.sample](mtu1280-netns.env.sample):

```sh
cp mtu1280-netns.env.sample mtu1280-netns.env
vi mtu1280-netns.env
```

Set `MTU1280NS_ADDR6_*` to the addresses of the routers. For IPv4, set
`MTU1280NS_ADDR4_*` as well. If you plan to use private IPv4 addresses, remember
to set port forwarding.

- Set `MTU1280NS_NO_OFFLOAD` to "true" to disable offloading features that
  can interfere with the packet capture result when debugging

```sh
# Stop http as the recipe will override some settings in httpd.service
sudo systemctl stop httpd.service

sudo make install

sudo systemctl enable --now mtu1280-netns.service httpd.service
```

```sh
# To see if the mtu1280-netns.sh script is alive and httpd is running
systemctl status mtu1280-netns.service httpd.service

# To understand and debug the magic yourself
sudo ip netns
sudo ip -6 addr
sudo ip -6 route
sudo ip -6 -n mtu1280-a addr
sudo ip -6 -n mtu1280-a route
sudo ip -6 -n mtu1280-b addr
sudo ip -6 -n mtu1280-b route
sudo ip -6 -n mtu1280-c addr
sudo ip -6 -n mtu1280-c route

# The listening ports will show up in the ns, not the default so the ports won't
#  show up in the ss command run without the ip or nsenter command
sudo ip netns exec mtu1280-c ss -tlnp
```

## Other Useful Info
All settings are netns-local.

- Use `ip -4 route show cache` and `ip -6 route show cache` to show PMTU cache
- Use `ip -6 route flush cache` and `ip -4 route flush cache` to flush the PMTU
  cache
- `sysctl -w net.ipv6.route.mtu_expires=0` and `sysctl -w
  net.ipv4.route.mtu_expires=0` disables PMTU caching
