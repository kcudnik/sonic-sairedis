#!/bin/bash

colorDefault="\033[01;00m"
colorGreenBlue="\033[104;92m"
colorBlackYellow="\033[103;30m"
colorBlackRed="\033[31;7m"
colorRed2="\033[6;38m"
colorRed="\033[66;91m"
colorGreen="\033[66;92m"
colorYellow="\033[66;93m"
colorBlue="\033[66;94m"
colorAqua="\033[66;96m"

killall syncd
killall orchagent
killall portsyncd
killall intfsyncd
killall neighsyncd
killall fpmsynd

# since redis server needs to run in switch ip namespace
killall redis-server

echo -e $colorGreen Remove all namespaces $colorDefault

ip netns list | while read ns; do
ip netns delete $ns
done

echo -e $colorGreen Setup switch namespaces $colorDefault

SWNAME=sw0

NS="ip netns exec $SWNAME "

ip netns add $SWNAME

$NS ip a a 127.0.0.1/8 dev lo
$NS ip a a ::1/128 dev lo
$NS ip l s dev lo up
$NS ping -c 1 127.0.0.1 >/dev/null 2>/dev/null

echo -e $colorGreen Seting up servers $colorDefault

SERVERS=31

seq 0 $SERVERS | while read srv; do

SRV="${SWNAME}srv$srv"

NSS="ip netns exec $SRV"

ip netns add $SRV

$NSS ip a a 127.0.0.1/8 dev lo
$NSS ip a a ::1/128 dev lo
$NSS ip l s dev lo up
$NSS ping -c 1 127.0.0.1 >/dev/null 2>/dev/null

# we could have TAP device created by server itself but just
# veth pair should be fine for now in server side

# add virtual link between

IF="${SWNAME}eth$((srv*4))"
IF="vEthernet$((srv*4))"

ip link add ${SRV}eth0 type veth peer name $IF
ip link set ${SRV}eth0 netns $SRV
ip link set $IF netns ${SWNAME}

$NSS ip l s dev ${SRV}eth0 up
$NS ip l s dev $IF up

# manually set ip addresses on server interfaces
# and they must belong to the subnet of corresponding interfaces EthernetX on switch
# defined inside /etc/network/interfaces

$NSS ip a a 10.0.0.$((srv*2+1))/31 dev ${SRV}eth0
$NSS route add default gw 10.0.0.$((srv*2))

done

echo -e $colorGreen Starting Redis DB in switch namespace $colorDefault

$NS /etc/rc3.d/S02redis_6379 restart

sleep 2

echo -e $colorGreen Flusshing Redis $colorDefault

$NS redis-cli flushall

echo -e $colorGreen Starting syncd $colorDefault

$NS syncd -uN -p brcm.profile.ini &

sleep 2

echo -e $colorGreen starting orchagent $colorDefault

$NS orchagent -m 00:11:11:11:11:22 &

sleep 2

echo -e $colorGreen Starting portsyncd $colorDefault

$NS portsyncd -p ./Force10-S6000/port_config.ini &

sleep 2

echo -e $colorGreen Starting Intfsyncd $colorDefault

$NS intfsyncd &

sleep 2

echo -e $colorGreen Starting neighsyncd $colorDefault

$NS neighsyncd &

sleep 2

# XXX currently interfaces.cfg is not used in switch namespace
# so we will set ip addresses manually

echo -e $colorGreen Set interfaces ip address and if up $colorDefault

seq 0 4 124 | while read all; do
$NS ip link set Ethernet$all up
$NS ip a a 10.0.0.$((all/2))/31 dev Ethernet$all
done

#$NS ifup --force -a

sleep 2

echo -e $colorGreen DONE $colorDefault

# perform tests
# - first ping srv0 to get arp entry
# - second insert route to app db to see if it's propagated to asic db

echo -e $colorGreen Inserting sample route to APP DB $colorDefault

$NS swssconfig sample.route.json

sleep 2 # give some time to propagate to ASIC DB

# WE NEED ARP for 10.0.0.1 to make this work

echo -e $colorGreen Ping from srv0 to srv1 to get arp on switch $colorDefault

ip netns exec sw0srv0 ping 10.0.0.3 -c 1

$NS arp -n

LINES=$($NS redis-cli -n 1 "keys" "*ROUTE_ENTRY:*2.2.2.0/24*" | grep ROUTE_ENTRY | wc -l)

if [ $LINES == 1 ];
then
    echo -e $colorGreen PASSED $colorDefault
else
    echo -e $colorRed FAILED, route not found in ASIC DB $colorDefault
    exit 1
fi

