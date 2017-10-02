#!/bin/bash

# TODO set MAC addresses on each interface

set -e

# remove all namespaces 

ip -all netns delete

echo srv1

ip netns add srv1
ip netns exec srv1 ip a a 127.0.0.1/8 dev lo
ip netns exec srv1 ip a a ::1/128 dev lo
ip netns exec srv1 ip l s dev lo up
ip netns exec srv1 ping -c 1 127.0.0.1 >/dev/null 2>/dev/null

echo srv2

ip netns add srv2
ip netns exec srv2 ip a a 127.0.0.1/8 dev lo
ip netns exec srv2 ip a a ::1/128 dev lo
ip netns exec srv2 ip l s dev lo up
ip netns exec srv2 ping -c 1 127.0.0.1 >/dev/null 2>/dev/null

echo sw1

ip netns add sw1
ip netns exec sw1 ip a a 127.0.0.1/8 dev lo
ip netns exec sw1 ip a a ::1/128 dev lo
ip netns exec sw1 ip l s dev lo up
ip netns exec sw1 ping -c 1 127.0.0.1 >/dev/null 2>/dev/null

echo links

ip link add srv1eth0 type veth peer name sw1eth0
ip link add srv2eth0 type veth peer name sw1eth1

ip link set srv1eth0 netns srv1
ip link set srv2eth0 netns srv2
ip link set sw1eth0 netns sw1
ip link set sw1eth1 netns sw1

echo routes

ip netns exec srv1 ip a a 10.1.0.1/24 dev srv1eth0
ip netns exec srv1 ip l s dev srv1eth0 up
ip netns exec srv1 route add default gw 10.1.0.2

ip netns exec srv2 ip a a 10.2.0.1/24 dev srv2eth0
ip netns exec srv2 ip l s dev srv2eth0 up
ip netns exec srv2 route add default gw 10.2.0.2


ip netns exec sw1 ip a a 10.1.0.2/24 dev sw1eth0
ip netns exec sw1 ip l s dev sw1eth0 up

ip netns exec sw1 ip a a 10.2.0.2/24 dev sw1eth1
ip netns exec sw1 ip l s dev sw1eth1 up

#ip netns exec sw1 route add -net 10.1.0.0/24 dev sw1eth0
#ip netns exec sw1 route add -net 10.2.0.0/24 dev sw1eth1

ip netns list

echo ping

# is switch reachable from srv 1
ip netns exec srv1 ping -c 1 10.1.0.2

# is switch reachable from srv 2
ip netns exec srv2 ping -c 1 10.2.0.2

# is srv 2 reachable from srv 1
ip netns exec srv1 ping -c 1 10.2.0.1
ip netns exec srv2 ping -c 1 10.1.0.1

# TODO add management interface to switch

ip netns exec srv1 ip a s
ip netns exec srv1 route -n
ip netns exec srv1 arp -n

ip netns exec srv2 ip a s
ip netns exec srv2 route -n
ip netns exec srv2 arp -n

ip netns exec sw1 ip a s
ip netns exec sw1 route -n
ip netns exec sw1 arp -n

