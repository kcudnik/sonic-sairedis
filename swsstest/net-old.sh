#!/bin/bash

# TODO set MAC addresses on each interface

set -e

# remove all namespaces 

ip -all netns delete

# create server 1 namespace

ip netns add srv1
ip netns exec srv1 ip a a 127.0.0.1/8 dev lo
ip netns exec srv1 ip a a ::1/128 dev lo
ip netns exec srv1 ip l s dev lo up
ip netns exec srv1 ping -c 1 127.0.0.1 >/dev/null 2>/dev/null

ip netns exec srv1 ip tuntap add dev srv1eth0 mode tap
ip netns exec srv1 ip a a 10.1.0.1/24 dev srv1eth0
ip netns exec srv1 ip l s dev srv1eth0 up
ip netns exec srv1 route add default dev srv1eth0

ip netns exec srv1 ip a s
ip netns exec srv1 route -n
ip netns exec srv1 arp -n

# create server 2 namespace

ip netns add srv2
ip netns exec srv2 ip a a 127.0.0.1/8 dev lo
ip netns exec srv2 ip a a ::1/128 dev lo
ip netns exec srv2 ip l s dev lo up
ip netns exec srv2 ping -c 1 127.0.0.1 >/dev/null 2>/dev/null

ip netns exec srv2 ip tuntap add dev srv2eth0 mode tap
ip netns exec srv2 ip a a 10.2.0.1/24 dev srv2eth0
ip netns exec srv2 ip l s dev srv2eth0 up
ip netns exec srv2 route add default dev srv2eth0

ip netns exec srv2 ip a s
ip netns exec srv2 route -n
ip netns exec srv2 arp -n


# create switch 1 namespace

ip netns add sw1
ip netns exec sw1 ip a a 127.0.0.1/8 dev lo
ip netns exec sw1 ip a a ::1/128 dev lo
ip netns exec sw1 ip l s dev lo up
ip netns exec sw1 ping -c 1 127.0.0.1 >/dev/null 2>/dev/null

ip netns exec sw1 ip tuntap add dev sw1eth0 mode tap
ip netns exec sw1 ip a a 10.1.0.2/24 dev sw1eth0
ip netns exec sw1 ip l s dev sw1eth0 up

ip netns exec sw1 ip tuntap add dev sw1eth1 mode tap
ip netns exec sw1 ip a a 10.2.0.2/24 dev sw1eth1
ip netns exec sw1 ip l s dev sw1eth1 up

# on the switch currently there is no default route

# XXX currently this setup is simple since both swich
# interfaces IP belongs to server subnet but this may
# not be the case in general, so we will need to
# add routing via IP or via interface (explicitly add)
# and this actually should be done via orchagent

ip netns exec sw1 route add -net 10.1.0.0/24 dev sw1eth0
ip netns exec sw1 route add -net 10.2.0.0/24 dev sw1eth1

# XXX issue when bridging interface when creating hostif interfaces
# this will probably need to be set up explicitly as physical link
# connection

ip netns exec sw1 ip a s
ip netns exec sw1 route -n
ip netns exec sw1 arp -n

ip netns list

# srv1eth0 -- sw1eth0
ip link add name br0 type bridge

# srv2eth0 -- sw1eth1
ip link add name br1 type bridge

# is switch reachable from srv 1
#ip netns exec srv1 ping 10.1.0.2

# is srv 2 reachable from srv 1
# ip netns exec srv1 ping 10.2.0.1

# is switch reachable from srv 2
#ip netns exec srv2 ping 10.2.0.2

# TODO add management interface to switch, and add mgmt interface to root namespace


# here we could create tap interface on that namespace to simulate ethernet

# now here we could create server application whethever

