# Flush mtu1280-c netns pmtu cache every second
`net.ipv4.route.mtu_expires` in a netns is a [recent
invention](https://github.com/torvalds/linux/commit/1de6b15a434c0068253fea5d719f71143e7e3a79).
With the system kernel without the patch, the following error will be shown when
staring `mtu1280-netns.service`:

```
sysctl: cannot stat /proc/sys/net/ipv4/route/mtu_expires: No such file or directory
```

As a cheap and quick hack, `mtu1280-netns-flush-cache.service` can be used to
achieve `net.ipv4.route.mtu_expires=0`.

```sh
systemctl enable --now mtu1280-netns-flush-cache.service
```

The service starts a shell script that flushes the mtu cache every second until
the netns is gone.
