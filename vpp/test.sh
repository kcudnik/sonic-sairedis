#!/bin/bash

# must be run from root

# vpp1host --- [ vpp1out vpp1vpp2 ] -- [ vpp2vpp1 vpp2out ] -- vpp2host
# 10.0.0.1/24 --- 10.0.0.2/24 = 10.0.3.1/24 --- 10.0.3.2/24 = 10.0.1.2/24 --- 10.0.1.1/24

systemctl stop vpp

function clean
{
    echo "* kill vpp"

    pgrep vpp|xargs kill -9 2>/dev/null
    #ps -ef |grep [v]pp

    echo "* remove links"
    
    ip link del vpp2vpp1 2>/dev/null
    ip link del vpp1vpp2 2>/dev/null
    ip link del vpp2out 2>/dev/null # not this ns?
    #ip link
}

clean;

echo "* remove all ns"

ip -all netns del

echo "* setting ns"

set -e

ip netns add vpp1
ip netns add vpp2
# ip netns list

sleep 0.1 # give time to clear vpp hosts

echo "* setup host interfaces"

ip -n vpp1 link add name vpp1host type veth peer name vpp1out
ip -n vpp1 link set vpp1out netns 1 # move to default ns
ip -n vpp1 addr add 10.0.0.1/24 dev vpp1host
ip -n vpp1 link set vpp1host up
ip -n vpp1 link set lo up

ip -n vpp2 link add name vpp2host type veth peer name vpp2out
ip -n vpp2 link set vpp2out netns 1 # move to default ns
ip -n vpp2 addr add 10.0.1.1/24 dev vpp2host
ip -n vpp2 link set vpp2host up
ip -n vpp2 link set lo up

ip link add name vpp1vpp2 type veth peer name vpp2vpp1 # in default NS
ip link set vpp1vpp2 up
ip link set vpp2vpp1 up
ip link set vpp1out up
ip link set vpp2out up

echo "* start vpp"

vpp -c startup1.conf
vpp -c startup2.conf
#ps -ef |grep [v]pp

sleep 0.1
VPP1="vppctl -s /run/vpp/cli-vpp1.sock"
VPP2="vppctl -s /run/vpp/cli-vpp2.sock"

echo DO C TEST
exit 0

# VPP: show api clients

$VPP1 create host name vpp1out
$VPP1 create host name vpp1vpp2
$VPP1 set int state host-vpp1out up
$VPP1 set int state host-vpp1vpp2 up
$VPP1 set int ip addr host-vpp1out 10.0.0.2/24
$VPP1 set int ip addr host-vpp1vpp2 10.0.3.1/24

$VPP2 create host name vpp2out
$VPP2 create host name vpp2vpp1
$VPP2 set int state host-vpp2out up
$VPP2 set int state host-vpp2vpp1 up
$VPP2 set int ip addr host-vpp2out 10.0.1.2/24
$VPP2 set int ip addr host-vpp2vpp1 10.0.3.2/24

echo "* add routes"

ip -n vpp1 route add 0.0.0.0/0 via 10.0.0.2
ip -n vpp2 route add 0.0.0.0/0 via 10.0.1.2

echo "* disable checksum" # for tcp to work
ip netns exec vpp1 ethtool -K vpp1host rx off tx off gso off >/dev/null
ip netns exec vpp2 ethtool -K vpp2host rx off tx off gso off >/dev/null

# normal route
#$VPP1 ip route add 10.0.1.0/24 via host-vpp1vpp2
#$VPP2 ip route add 10.0.0.0/24 via host-vpp2vpp1

echo "* create tunnels"
$VPP1 create ipip tunnel src 10.0.3.1 dst 10.0.3.2
$VPP1 set int state ipip0 up
$VPP1 ip route add 10.0.1.0/24 via ipip0
$VPP1 set int ip addr ipip0 1.1.1.1/32

$VPP2 create ipip tunnel src 10.0.3.2 dst 10.0.3.1
$VPP2 set int state ipip0 up
$VPP2 ip route add 10.0.0.0/24 via ipip0
$VPP2 set int ip addr ipip0 1.1.1.1/32

echo "* test ping"
#$VPP2 ping 10.0.1.1
#$VPP2 ping 10.0.3.1
#$VPP2 ping 10.0.0.2
#ip netns exec vpp1 ping 10.0.0.1 -c 1
ip netns exec vpp1 ping 10.0.1.1 -c 4 # reach vpp2
ip netns exec vpp2 ping 10.0.0.1 -c 4 # reach vpp1

# test ip in ip with tcp dump

#clear
exit;


ip netns exec vpp2 ping 10.0.0.1
ip netns exec vpp1 nc -l 2222
ip netns exec vpp2 nc 10.0.0.1 2222

tcpdump -n -i vpp1vpp2 -i vpp2vpp1





