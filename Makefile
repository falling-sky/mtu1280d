
help:
	@echo "make mtu1280d - just build binary"
	@echo "make install - installs to /usr/sbin/mtu1280d"
	@echo "make upstart - installs, including /etc/init/ script"
	@echo "make init.d - installs, including /etc/init.d/ and runs chkconfig"
	@test -d /etc/init && echo "we recommend make upstart (we found /etc/init)" || true
	@test ! -d /etc/init && echo "we recommend make init.d (we did not find /etc/init)" || true

mtu1280d: mtu1280d.c
	gcc -o mtu1280d mtu1280d.c -lnetfilter_queue 

install: mtu1280d
	/usr/bin/install -c mtu1280d /usr/sbin/

upstart: install
	@echo Checking to see if your system uses upstart 
	test -d /etc/init/
	/usr/bin/install -c upstart/mtu1280d /etc/init/

init.d: install
	/usr/bin/install -c init.d/mtu1280d /etc/init.d/
	update-rc.d mtu1280d defaults
	update-rc.d mtu1280d enable
remove:
	@echo "Removing any previous installation (including startup scripts) of mtu1280d"
	rm -fr /usr/sbin/mtu1280d /etc/init/mtu1280d /etc/init.d/mtu1280d
	update-rc.d mtu1280d remove

