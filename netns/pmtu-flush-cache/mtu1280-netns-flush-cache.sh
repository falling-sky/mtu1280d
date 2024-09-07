#!/bin/sh
a=0
b=0

while [ "$a" -eq 0 ] || [ "$b" -eq 0 ]
do
	sleep 1

	ip -n mtu1280-c -4 route flush cache 2> /dev/null >/dev/null
	a=$?
	ip -n mtu1280-c -6 route flush cache 2> /dev/null >/dev/null
	b=$?
done
