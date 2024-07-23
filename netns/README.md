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
$ tracepath fd12:34::3:1500:0
 1?: [LOCALHOST]                        0.025ms pmtu 1500
 1:  fd12:34::1:1280:0                                     0.128ms
 1:  fd12:34::1:1280:0                                     0.156ms
 2:  fd12:34::1:1280:0                                     0.244ms <b>pmtu 1280</b>
 2:  fd12:34::2:1280:0                                     0.235ms
 3:  fd12:34::3:1500:0                                     0.188ms reached
     Resume: pmtu 1280 hops 3 back 3
</pre>

3 netns are utilised so that the MTU 1280 links are hidden from both the
internet facing gateway and httpd. This forces httpd to announce its MSS as 1440
octets and PMTUD is carried out for each endpoint from the internet. The PMTU
caching in **mtu1280-c** is disabled so that the test clients can get the same
result all the time. The makefile recipe installs a "drop-in" service unit
config to override `NetworkNamespacePath` so httpd is launched in the mtu1280-c
netns.

The set up should perfectly simulate the kernel's behaviour. A few down sides
over the original mtu1280d approach are

1. more overhead as all 3 netns along the path need to be "simulated" -
   routing table, neighbor cache, PMTU cache ...
1. somewhat difficult to integrate netns to most programs as they're not
   designed with namespace in mind
1. complexity making it difficult to troubleshoot

## Network Set Up
See [/um/README.md#network-set-up](/um/README.md#network-set-up).

## INSTALL
Copy and edit the env file [mtu1280-netns.env.sample](mtu1280-netns.env.sample):

```sh
cp mtu1280-netns.env.sample mtu1280-netns.env
vi mtu1280-netns.env
```

Set `MTU1280NS_ADDR_A` to the address of the first router and so on.

- Use `MTU1280NS_ADDR_GW` if you want the script to set the IPv6 address of the
  host (it is best if it's left with NetworkManager, though)
- Set `MTU1280NS_NO_OFFLOAD` to "true" to disable offloading features that
  interfere with the packet capture result (for debugging)

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

# The PMTU cache table entries will pile up in production
sudo ip -6 -n mtu1280-c route show cache
```

## Other Useful Info
All settings are netns-local.

- `ip -6 route flush cache` flushes the PMTU cache
- `sysctl -w net.ipv6.route.mtu_expires=0` disables PMTU caching
