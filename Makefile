
help:
	@echo "make mtu1280d - just build binary"
	@echo "make install - installs to /usr/sbin/mtu1280d"
	@echo "make upstart - installs, including /etc/init/ script"
	@echo "make init.d - installs, including /etc/init.d/ and runs chkconfig"
	@test -d /etc/init && echo "we recommend make upstart (we found /etc/init)" || true
	@test ! -d /etc/init && echo "we recommend make init.d (we did not find /etc/init)" || true

mtu1280d: mtu1280d.c
	gcc -o mtu1280d mtu1280d.c -lnetfilter_queue  || ( echo "see README.md for prerequisites" && exit 1 )

test: mtu1280d
	sudo ./mtu1280d -g

clean:
	rm -f mtu1280d

install: mtu1280d
	/usr/bin/install -c mtu1280d /usr/sbin/

upstart: install
	@echo Checking to see if your system uses upstart 
	test -d /etc/init/
	/usr/bin/install -c upstart/mtu1280d /etc/init/
	@echo "Reminder - start the daemon or reboot; then update your ip6tables."
	@echo "See the README.md file."

init.d: install
	/usr/bin/install -c init.d/mtu1280d /etc/init.d/
	update-rc.d mtu1280d defaults
	update-rc.d mtu1280d enable
	@echo "Reminder - start the daemon or reboot; then update your ip6tables."
	@echo "See the README.md file."


################################################################
# Removing? Check ip6tables first                              #
################################################################

pre-remove:
	@echo Checking ip6tables to make sure you no longer use NFQUEUE
	@ip6tables-save | grep NFQUEUE  && echo "NFQUEUE rulee still active in ip6tables"  || true
	@ip6tables-save | grep "NFQUEUE" >/dev/null ;  [ $$? -eq 1 ]
	@echo safe to remove
	
force-remove:
	@echo "Removing any previous installation (including startup scripts) of mtu1280d"
	rm -fr /usr/sbin/mtu1280d /etc/init/mtu1280d /etc/init.d/mtu1280d
	update-rc.d mtu1280d remove


remove: pre-remove force-remove









################################################################
# Used by Jason for releases via rsync                         #
################################################################

dist-prep::
	rm -fr work
	mkdir -p work
	rsync -av . work --exclude work --exclude "*~" --exclude /mtu1280d --exclude ".git"

dist-test: dist-prep
	../dist_support/make-dist.pl --stage work --base mtu1280d --branch test

dist-stable: dist-prep
	../dist_support/make-dist.pl --stage work --base mtu1280d --branch stable

