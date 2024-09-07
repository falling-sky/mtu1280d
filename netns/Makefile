.PHONY: uninstall

install:
	install -m 644 -D mtu1280-netns.env /etc/mtu1280/mtu1280-netns.env
	install -m 644 -b -D mtu1280-netns-httpd.override.conf /etc/systemd/system/httpd.service.d/mtu1280-netns-httpd.override.conf
	install -m 644 -D mtu1280-netns.service /usr/local/lib/systemd/system/mtu1280-netns.service
	install -m 644 -D pmtu-flush-cache/mtu1280-netns-flush-cache.service /usr/local/lib/systemd/system/mtu1280-netns-flush-cache.service
	install -m 755 -D pmtu-flush-cache/mtu1280-netns-flush-cache.sh /usr/local/bin/mtu1280-netns-flush-cache.sh
	install -m 755 -D mtu1280-netns.sh /usr/local/libexec/mtu1280/mtu1280-netns.sh
	systemctl daemon-reload

uninstall:
	rm -f \
		/etc/systemd/system/httpd.service.d/mtu1280-netns-httpd.override.conf \
		/usr/local/lib/systemd/system/mtu1280-netns.service \
		/usr/local/lib/systemd/system/mtu1280-netns-flush-cache.service \
		/usr/local/libexec/mtu1280/mtu1280-netns.sh \
		/usr/local/bin/mtu1280-netns-flush-cache.sh
	systemctl daemon-reload
