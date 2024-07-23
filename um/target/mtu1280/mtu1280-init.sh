#!/bin/sh
set -e
echo $$ > /run/mtu1280-init.pid

apk update
apk upgrade
apk add \
	openrc \
	eudev-openrc \
	apache2 \
	apache2-ssl \
	apache2-proxy \
	apache2-dev \
	git \
	gcc \
	make \
	libc-dev \
	logrotate \
	dropbear \
	dropbear-ssh

cd /opt/mod_ip
./configure
make -j$(nproc)
make install

rc-update add apache2 default
service apache2 start

rm -f /etc/runlevels/*/mtu1280-init
