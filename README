mtu1280 - emulates serving via a low MTU IPv6 tunnel
----------------------------------------------------

This program is will generate ICMPv6 "Packet Too Big"
responses with an MTU of 1280. mtu1280 will connect to 
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

Example rule:

```
guest% sudo ip6tables-save | grep NFQ
-A INPUT -d 2001:470:1f04:d63::2/128 -m length --length 1281:65535 -j -NFQUEUE --queue-num 1280
```


LICENSE
-------
GPLv2, due to  the duplicated code from  Hararld Welte's
libnetfilter_queue-1.0.2/utils/nfqnl_test.c (included).


