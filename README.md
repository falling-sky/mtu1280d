mtu1280d - emulates serving via a low MTU IPv6 tunnel
----------------------------------------------------

This program is will generate ICMPv6 "Packet Too Big"
responses with an MTU of 1280. mtu1280d will connect to 
a netfilter_queue socket, listening for packets; and 
respond to all packets sent to that queue.

This is meant to be ran on a secondary IP for your host.
It is recommend that your primary IP is NOT used with
this technique in case of application failure.

To deploy, compile build and install.  Copy
in one of the init or init.d scripts, and make sure
it is set for auto-start for your OS.  An actual reboot
is recommended.

Once up and running, configure ip6tables to route
large packets destined to the desired IP to the netfilter queue.

Example rules:

```
iptables -t mangle -A PREROUTING -d 2001:db1::1280/128 -j NFQUEUE --queue-num 1280
```

mtu1280d will, when it sees a packet > 1280 bytes long, 
both reject the packet as well as generate an ICMPv6 Packet Too Big
back to the sender.

RECOMMENDATION
--------------
Apply this to a dedicated address specifically for triggering mtu 1280.
Configure your interface via /etc/rc.local, with a command such as this:

```
ip -6 addr add 2001:db8:1:18::1280 dev eth0 preferred_lft 0
```

The `preferred_lft 0` is important to mark the address as a deprecated address.
This means only use the address for incoming connections; not for outgoing.


UBUNTU 18 NOTES ON NFQUEUE HANGS
--------------------------------

We're seeing reports of the daemon wedging.  So far, my observations
on my own ubuntu 18 system are that the recv() calls against the
iptables nfqueue hang.

The master branch (not pushed to the rsync server) specifically
adds in a watchdog function; after a configurable numbrer of seconds,
it will disconnect the nfqueue and reattach.  If it does this
too many times, it will abort.

You can tune this with these options:

     -w 60    - How long we can go without seeing a packet
     -W 1440  - How many times we can reset the socket without seeing a pocket

For most of you, I'm monitoring your web sites. At minimum I should
be hitting your mirror once every 30 minutes; somehow you should
see and accept traffic in the time above (1 day!).


REQUIREMENTS
------------

RedHat/Centos/Fedora:
 * libnetfilter_queue-devel
 * gcc, make
 * ip6tables - and a way to automatically load ip6tables on startup

Ubuntu/Debian:
 * build-essential 
 * libnetfilter-queue-dev 
 * ip6tables - and a way to automatically load ip6tables on startup


IPTABLES / IP6TABLES
--------------------

For reference, this is what jfesler does:

/etc/rc.local:
```
iptables-restore /etc/iptables/rules.v4
ip6tables-restore /etc/iptables/rules.v6
```

/etc/iptables/rules.v6 (simplified version, only includes mtu1280d rule)
```
# Generated by ip6tables-save v1.4.21 on Wed Feb 18 10:14:54 2015
*mangle
:PREROUTING ACCEPT [0:0]
:INPUT ACCEPT [0:0]
:FORWARD ACCEPT [0:0]
:OUTPUT ACCEPT [0:0]
:POSTROUTING ACCEPT [0:0]
-A PREROUTING -d 2001:db8::1280/128 -j NFQUEUE --queue-num 1280
COMMIT
# Completed on Wed Feb 18 10:14:54 2015
# Generated by ip6tables-save v1.4.21 on Wed Feb 18 10:14:54 2015
*filter
:INPUT ACCEPT [0:0]
:FORWARD ACCEPT [0:0]
:OUTPUT ACCEPT [0:0]
:CHECK_ABUSE - [0:0]
COMMIT
# Completed on Wed Feb 18 10:14:54 2015
```


LICENSE
-------
GPLv2, due to  the duplicated code from  Hararld Welte's
libnetfilter_queue-1.0.2/utils/nfqnl_test.c (included).


