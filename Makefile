
mtu1280: mtu1280d.c
	gcc -o mtu1280 mtu1280d.c -lnetfilter_queue 

install: mtu1280d
	/usr/bin/install -c mtu1280d /usr/sbin/mtu1280d
		