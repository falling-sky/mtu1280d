KVER = 6.6.40
KVER_MAJOR = 6
ALPINE_VER = 3.20.1-x86_64
ALPINE_VER_MAJOR = 3.20
APK_STATIC_URL = https://gitlab.alpinelinux.org/api/v4/projects/5/packages/generic/v2.12.10/x86_64/apk.static
NPROC = $(shell nproc)
# for safety because otherwise the -j option will have no value assigned and
# make will go berserk
NPROC ?= 1

.PHONY: first clean uninstall clean-rootimg rebuild-rootimg run-uml uninstall-all

first: build/linux-$(KVER)_mtu1280/linux build/alpine-minirootfs-$(ALPINE_VER).tar.gz build/apk.static
rootimg: build/root.img
rebuild-rootimg: clean-rootimg rootimg

-include /etc/mtu1280/mtu1280-uml.env
run-uml:
	/usr/local/bin/linux ${MTU1280_UML_OPTS} ${MTU1280_UML_OPTS_EXTRA}

clean:
	rm -rf build

clean-rootimg:
	rm -f build/root.img

install: build/linux-$(KVER)_mtu1280/linux build/root.img
	mkdir -p /usr/local/bin
	mkdir -p /var/lib/mtu1280
	mkdir -p /usr/local/lib/systemd/system
	mkdir -p /etc/mtu1280
	install -o 0 -g 0 -m 755 -Z build/linux-$(KVER)_mtu1280/linux /usr/local/bin
	install -b -o 0 -g 0 -m 644 -Z build/root.img /var/lib/mtu1280
	test -f /etc/mtu1280/mtu1280-uml.env || \
		install -b -o 0 -g 0 -m 644 -Z host/mtu1280-uml.env /etc/mtu1280
	install -o 0 -g 0 -m 644 -Z host/mtu1280-uml.service /usr/local/lib/systemd/system
	install -o 0 -g 0 -m 755 -Z host/dispatcher.d/mtu1280-hook /etc/NetworkManager/dispatcher.d
	install -o 0 -g 0 -m 644 -Z host/sysctl.d/10-mtu1280-forwarding.conf /etc/sysctl.d
	sysctl -qw --system
	systemctl daemon-reload

install-radvd:
	install -b -o 0 -g 0 -m 644 -Z host/radvd.conf /etc

uninstall:
	rm -f \
		/usr/local/bin/linux \
		/usr/local/lib/systemd/system/mtu1280-uml.service \
		/etc/NetworkManager/dispatcher.d/mtu1280-hook \
		/etc/sysctl.d/10-mtu1280-forwarding.conf
	systemctl daemon-reload

uninstall-all: uninstall
	rm -rf /var/lib/mtu1280

build/linux-$(KVER).tar.xz:
	mkdir -p build
	cd build && wget -c \
		https://cdn.kernel.org/pub/linux/kernel/v$(KVER_MAJOR).x/linux-$(KVER).tar.xz \
		https://cdn.kernel.org/pub/linux/kernel/v$(KVER_MAJOR).x/linux-$(KVER).tar.sign || \
			rm -f linux-$(KVER).tar.xz linux-$(KVER).tar.sign
# TODO: verify signature?

build/linux-$(KVER)/MTU1280_MARKER: build/linux-$(KVER).tar.xz
	rm -rf linux-$(KVER)
	cd build && tar xf linux-$(KVER).tar.xz || rm -rf linux-$(KVER)
	touch build/linux-$(KVER)/MTU1280_MARKER

build/linux-$(KVER)_mtu1280/linux: build/linux-$(KVER)/MTU1280_MARKER
	mkdir -p build/linux-$(KVER)_mtu1280
	ln -fs ../../linux-x86_64.config build/linux-$(KVER)_mtu1280/.config
	cd build/linux-$(KVER)_mtu1280 && \
		make -f ../linux-$(KVER)/Makefile ARCH=um -j$(NPROC)

build/alpine-minirootfs-$(ALPINE_VER).tar.gz:
	mkdir -p build
	cd build && wget -c \
		https://dl-cdn.alpinelinux.org/alpine/v$(ALPINE_VER_MAJOR)/releases/x86_64/alpine-minirootfs-$(ALPINE_VER).tar.gz \
		https://dl-cdn.alpinelinux.org/alpine/v$(ALPINE_VER_MAJOR)/releases/x86_64/alpine-minirootfs-$(ALPINE_VER).tar.gz.sha256 \
		https://dl-cdn.alpinelinux.org/alpine/v$(ALPINE_VER_MAJOR)/releases/x86_64/alpine-minirootfs-$(ALPINE_VER).tar.gz.asc || \
			rm -f \
				alpine-minirootfs-$(ALPINE_VER).tar.gz \
				alpine-minirootfs-$(ALPINE_VER).tar.gz.sha256 \
				alpine-minirootfs-$(ALPINE_VER).tar.gz.asc
# TODO: verify signature?

build/apk.static:
	wget "${APK_STATIC_URL}" -cO build/apk.static
	chmod 755 build/apk.static

build/root.img: build/apk.static build/alpine-minirootfs-$(ALPINE_VER).tar.gz build/linux-$(KVER)_mtu1280/linux
ifneq ($(shell id -u), 0)
	@echo "The target requires root." >&2 && exit 2
endif
	rm -f build/root.img.tmp
	rm -rf build/root
	mkdir build/root

	tar -C build/root -xf build/alpine-minirootfs-$(ALPINE_VER).tar.gz

	cd build/linux-$(KVER)_mtu1280 && \
		make -f ../linux-$(KVER)/Makefile ARCH=um INSTALL_MOD_PATH=../root modules_install

	echo -e "# UML console's on tty0 without login\ntty0::respawn:/bin/sh\n" \
		>> build/root/etc/inittab

	cp -f target/hostname build/root/etc
	cp -f target/interfaces build/root/etc/network
	cp -f target/resolv.conf build/root/etc

	mkdir -p build/root/etc/apache2/conf.d
	cp target/apache/ip.conf target/apache/site-mtu1280.conf build/root/etc/apache2/conf.d

	mkdir -p build/root/usr/local/libexec/mtu1280 build/root/etc/init.d
	cp target/mtu1280/mtu1280-init.sh build/root/usr/local/libexec/mtu1280/mtu1280-init.sh
	chmod 755 build/root/usr/local/libexec/mtu1280/mtu1280-init.sh
	cp target/mtu1280/mtu1280-init build/root/etc/init.d
	chmod 755 build/root/etc/init.d/mtu1280-init

	build/apk.static --root build/root add openrc
# Github.com doesn't do IPv6 and does IPv4 with DF bit always set so the host
# is unable to fragment the packets.
# Might as well ship the repo with the image.
	mkdir -p build/root/opt
	git -C build/root/opt clone https://github.com/falling-sky/mod_ip

	mkdir -p build/root/etc/runlevels/default
	ln -fs /etc/init.d/hostname build/root/etc/runlevels/default/hostname
	ln -fs /etc/init.d/networking build/root/etc/runlevels/default/networking
	ln -fs /etc/init.d/fsck build/root/etc/runlevels/default/fsck
	ln -fs /etc/init.d/mtu1280-init build/root/etc/runlevels/default/mtu1280-init

	fallocate -l 2G build/root.img.tmp
	mkfs.ext4 -d build/root build/root.img.tmp
	mv build/root.img.tmp build/root.img
