#!/bin/bash

# TODO set MAC addresses on each interface

# XXX redis db must be running inside switch namespace
# ip netns exec sw1 /etc/rc3.d/S20redis_6379 restart

# XXX /etc/network/interfaces will not operate inside name space

# current setup:
# server X veth0 (ip/31) -- switch 1 -- veth srvXeth0 -- tap EthernetX

killall fakesyncd
killall ping
killall tcpdump 

set -e

# remove all namespaces 

#ip -all netns delete
ip netns list | while read ns; do
ip netns delete $ns
done

# setup switch namespace

ip netns add sw1
ip netns exec sw1 ip a a 127.0.0.1/8 dev lo
ip netns exec sw1 ip a a ::1/128 dev lo
ip netns exec sw1 ip l s dev lo up
ip netns exec sw1 ping -c 1 127.0.0.1 >/dev/null 2>/dev/null

# setup some servers

SERVERS=1

seq 0 $SERVERS | while read srv; do 

echo setting up server $srv

SRV="srv$srv"

ip netns add $SRV
ip netns exec $SRV ip a a 127.0.0.1/8 dev lo
ip netns exec $SRV ip a a ::1/128 dev lo
ip netns exec $SRV ip l s dev lo up
ip netns exec $SRV ping -c 1 127.0.0.1 >/dev/null 2>/dev/null

# we could have TAP device created by server itself but just
# veth pair should be fine for now in server side

# add virtual link between 

ip link add ${SRV}eth0 type veth peer name sw1eth$((srv*4))
ip link set ${SRV}eth0 netns $SRV
ip link set sw1eth$((srv*4)) netns sw1

ip netns exec $SRV ip l s dev ${SRV}eth0 up
ip netns exec sw1 ip l s dev sw1eth$((srv*4)) up

# manually set ip addresses on server interfaces
# and they must belong to the subnet of corresponding interfaces EthernetX on switch
# defined inside /etc/network/interfaces

ip netns exec $SRV ip a a 10.0.0.$((srv*2+1))/31 dev ${SRV}eth0
ip netns exec $SRV route add default gw 10.0.0.$((srv*2))

done

ip netns list

seq 0 $SERVERS | while read srv; do 

SRV="srv$srv"

echo printing server $SRV

ip netns exec $SRV ip a s
ip netns exec $SRV route -n
ip netns exec $SRV arp -n

done

echo Switch NS info

ip netns exec sw1 ip a s
set +e
ip netns exec sw1 route -n
set -e
ip netns exec sw1 arp -n

echo now run the test to setup ethernet interfaces

echo starting fake syncd
ip netns exec sw1 ./fakesyncd &

sleep 5

seq 0 4 124 | while read all; do
ip netns exec sw1 ip link set Ethernet$all up
ip netns exec sw1 ip a a 10.0.0.$((all/2))/31 dev Ethernet$all
done

echo DONE

# now test if servers can ping each other and then
# if servers can send data to each other
