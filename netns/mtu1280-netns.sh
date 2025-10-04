#!/bin/sh
[ -z "$MTU1280NS_NO_OFFLOAD" ] && MTU1280NS_NO_OFFLOAD=false

safe_run () {
	if [ -z "$1" ]; then
		return
	fi

	shift
	$@
}

mkns () {
	set -e
	local ll

	do_make_ns () {
		ip netns add "$1"
		ip netns exec "$1" sysctl -qw \
			"net.ipv4.conf.all.forwarding=1" \
			"net.ipv4.conf.default.forwarding=1" \
			"net.ipv4.ip_forward=1" \
			"net.ipv6.conf.default.forwarding=1" \
			"net.ipv6.conf.all.forwarding=1"
	}

	get_ll_addr () {
		local ll

		while true
		do
			if [ -z "$2" ]; then
				ll=$(ip -br -6 addr show dev $1 scope link | grep -Eoi 'fe[^\s/]+')
			else
				ll=$(ip -br -6 -n $2 addr show dev $1 scope link | grep -Eoi 'fe[^\s/]+')
			fi

			if [ -z "$ll" ]; then
				sleep 0.1
			else
				echo "$ll"
				break
			fi
		done
	}

	filter_cidr () {
		echo "${1%/*}"
	}

	no_offload () {
		if ! "$MTU1280NS_NO_OFFLOAD"; then
			return
		fi

		if [ -z "$2" ]; then
			ethtool -K "$1" tcp-segmentation-offload off generic-segmentation-offload off generic-receive-offload off
		else
			ip netns exec "$2" ethtool -K "$1" tcp-segmentation-offload off generic-segmentation-offload off generic-receive-offload off
		fi
	}

	do_make_ns mtu1280-a
	do_make_ns mtu1280-b
	do_make_ns mtu1280-c

	# link up system default ns -> mtu1280-a
	ip link add veth-mtu1280-a type veth peer name veth-mtu1280-gw netns mtu1280-a
	no_offload veth-mtu1280-a
	no_offload veth-mtu1280-gw mtu1280-a
	# link up mtu1280-a -> mtu1280-b
	ip -n mtu1280-a link add veth-mtu1280-b type veth peer name veth-mtu1280-a netns mtu1280-b
	no_offload veth-mtu1280-b mtu1280-a
	no_offload veth-mtu1280-a mtu1280-b
	# link up mtu1280-b -> mtu1280-c
	ip -n mtu1280-b link add veth-mtu1280-c type veth peer name veth-mtu1280-b netns mtu1280-c
	no_offload veth-mtu1280-c mtu1280-b
	no_offload veth-mtu1280-b mtu1280-c

	safe_run "$MTU1280NS_ADDR4_A0" ip -4 addr add "$MTU1280NS_ADDR4_A0" dev veth-mtu1280-a

	safe_run "$MTU1280NS_ADDR4_A1" ip -n mtu1280-a -4 addr add "$MTU1280NS_ADDR4_A1" dev veth-mtu1280-gw
	safe_run "$MTU1280NS_ADDR4_B0" ip -n mtu1280-a -4 addr add "$MTU1280NS_ADDR4_B0" dev veth-mtu1280-b

	safe_run "$MTU1280NS_ADDR4_B1" ip -n mtu1280-b -4 addr add "$MTU1280NS_ADDR4_B1" dev veth-mtu1280-a
	safe_run "$MTU1280NS_ADDR4_C0" ip -n mtu1280-b -4 addr add "$MTU1280NS_ADDR4_C0" dev veth-mtu1280-c

	safe_run "$MTU1280NS_ADDR4_C1" ip -n mtu1280-c -4 addr add "$MTU1280NS_ADDR4_C1" dev veth-mtu1280-b

	safe_run "$MTU1280NS_ADDR6_A" ip -n mtu1280-a -6 addr add "$MTU1280NS_ADDR6_A" dev veth-mtu1280-gw
	safe_run "$MTU1280NS_ADDR6_B" ip -n mtu1280-b -6 addr add "$MTU1280NS_ADDR6_B" dev veth-mtu1280-a
	safe_run "$MTU1280NS_ADDR6_C" ip -n mtu1280-c -6 addr add "$MTU1280NS_ADDR6_C" dev veth-mtu1280-b

	ip link set veth-mtu1280-a up
	ip -n mtu1280-a link set lo up
	ip -n mtu1280-a link set veth-mtu1280-gw up

	ip -n mtu1280-a link set veth-mtu1280-b up
	ip -n mtu1280-b link set lo up
	ip -n mtu1280-b link set veth-mtu1280-a up

	ip -n mtu1280-b link set veth-mtu1280-c up
	ip -n mtu1280-c link set lo up
	ip -n mtu1280-c link set veth-mtu1280-b up

	# default route from mtu1280-a to system
	safe_run "$MTU1280NS_ADDR4_A0" ip -n mtu1280-a -4 route add default dev veth-mtu1280-gw via $(filter_cidr "$MTU1280NS_ADDR4_A0")
	ip -n mtu1280-a -6 route add default dev veth-mtu1280-gw via $(get_ll_addr veth-mtu1280-a)
	# default route from mtu1280-b to mtu1280-a (mtu 1280 segment)
	safe_run "$MTU1280NS_ADDR4_B0" ip -n mtu1280-b -4 route add default dev veth-mtu1280-a via $(filter_cidr "$MTU1280NS_ADDR4_B0") mtu 1280
	ip -n mtu1280-b -6 route add default dev veth-mtu1280-a via $(get_ll_addr veth-mtu1280-b mtu1280-a) mtu 1280
	# default route from mtu1280-c to mtu1280-b
	safe_run "$MTU1280NS_ADDR4_C0" ip -n mtu1280-c -4 route add default dev veth-mtu1280-b via $(filter_cidr "$MTU1280NS_ADDR4_C0")
	ip -n mtu1280-c -6 route add default dev veth-mtu1280-b via $(get_ll_addr veth-mtu1280-c mtu1280-b)

	# static route from system default ns to inner
	ll=$(get_ll_addr veth-mtu1280-gw mtu1280-a)
	safe_run "$MTU1280NS_ADDR4_B0" ip -4 route add "$(filter_cidr "$MTU1280NS_ADDR4_B0")" dev veth-mtu1280-a via "$(filter_cidr "$MTU1280NS_ADDR4_A1")"
	safe_run "$MTU1280NS_ADDR4_B1" ip -4 route add "$(filter_cidr "$MTU1280NS_ADDR4_B1")" dev veth-mtu1280-a via "$(filter_cidr "$MTU1280NS_ADDR4_A1")"
	safe_run "$MTU1280NS_ADDR4_C0" ip -4 route add "$(filter_cidr "$MTU1280NS_ADDR4_C0")" dev veth-mtu1280-a via "$(filter_cidr "$MTU1280NS_ADDR4_A1")"
	safe_run "$MTU1280NS_ADDR4_C1" ip -4 route add "$(filter_cidr "$MTU1280NS_ADDR4_C1")" dev veth-mtu1280-a via "$(filter_cidr "$MTU1280NS_ADDR4_A1")"
	safe_run "$MTU1280NS_ADDR6_A" ip -6 route add "$MTU1280NS_ADDR6_A" dev veth-mtu1280-a via $ll
	safe_run "$MTU1280NS_ADDR6_B" ip -6 route add "$MTU1280NS_ADDR6_B" dev veth-mtu1280-a via $ll
	safe_run "$MTU1280NS_ADDR6_C" ip -6 route add "$MTU1280NS_ADDR6_C" dev veth-mtu1280-a via $ll
	# static route from mtu1280-a to inner (mtu 1280 segment)
	ll=$(get_ll_addr veth-mtu1280-a mtu1280-b)
	safe_run "$MTU1280NS_ADDR4_C0" ip -n mtu1280-a -4 route add "$(filter_cidr "$MTU1280NS_ADDR4_C0")" dev veth-mtu1280-b via "$(filter_cidr "$MTU1280NS_ADDR4_B1")" mtu 1280
	safe_run "$MTU1280NS_ADDR4_C1" ip -n mtu1280-a -4 route add "$(filter_cidr "$MTU1280NS_ADDR4_C1")" dev veth-mtu1280-b via "$(filter_cidr "$MTU1280NS_ADDR4_B1")" mtu 1280
	safe_run "$MTU1280NS_ADDR6_B" ip -n mtu1280-a -6 route add "$MTU1280NS_ADDR6_B" dev veth-mtu1280-b via $ll mtu 1280
	safe_run "$MTU1280NS_ADDR6_C" ip -n mtu1280-a -6 route add "$MTU1280NS_ADDR6_C" dev veth-mtu1280-b via $ll mtu 1280
	# static route from mtu1280-b to inner
	ll=$(get_ll_addr veth-mtu1280-b mtu1280-c)
	safe_run "$MTU1280NS_ADDR6_C" ip -n mtu1280-b -6 route add "$MTU1280NS_ADDR6_C" dev veth-mtu1280-c via $ll

	# disable PMTU caching in mtu1280-c
	ip netns exec mtu1280-c sysctl -qw "net.ipv4.route.mtu_expires=0" || true
	ip netns exec mtu1280-c sysctl -qw "net.ipv6.route.mtu_expires=0" || true
}

rmns () {
	ip link del veth-mtu1280-a type veth
	ip -n mtu1280-a link del veth-mtu1280-b type veth
	ip -n mtu1280-b link del veth-mtu1280-c type veth

	ip netns del mtu1280-a
	ip netns del mtu1280-b
	ip netns del mtu1280-c
}

rmns_q () {
	rmns $@ 2> /dev/null > /dev/null
}

daemon () {
	mkns

	systemd-notify --status="Stopped and holding netns"
	systemd-notify --ready
	kill -STOP 0

	systemd-notify --stopping
	systemd-notify --status="Deleting netns"
	rmns

	systemd-notify --status=""
}

cmd="$1"
shift
"$cmd" $@
