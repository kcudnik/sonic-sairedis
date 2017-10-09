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

# remove interfaces, TODO later on they should not be persiten on tests
echo -e $colorGreen Remove Ethernet interfaces $colorDefault
seq 0 4 124| while read all; do ip tuntap del name Ethernet$all mode tap; done

set -e

if [ $0 != "" ]; then

    echo -e $colorGreen Flusshing Redis $colorDefault
    redis-cli flushall
fi

# we need sudo since we create tap interface
echo -e $colorGreen starting syncd $colorDefault
syncd -uN -p brcm.profile.ini &

sleep 2

echo -e $colorGreen starting orchagent $colorDefault
orchagent -m 00:11:11:11:11:22 &

sleep 2

echo -e $colorGreen starting portsyncd $colorDefault
portsyncd -p ./Force10-S6000/port_config.ini &

sleep 2

echo -e $colorGreen starting intfsyncd $colorDefault
intfsyncd &

sleep 2

echo -e $colorGreen starting neighsyncd $colorDefault
neighsyncd &

sleep 2

echo set interfaces ip address and if up

seq 0 4 124 | while read all; do
ip link set Ethernet$all up
ip a a 10.0.0.$((all/2))/31 dev Ethernet$all
done

echo -e $colorGreen DONE $colorDefault
exit 0

# TODO remove arp set
# TODO combine test2 and net2 to single test
# TODO add create hostif
# TODO add ping test

echo -e $colorGreen populate static ARP $colorDefault
seq 1 2 63| while read all; do 
HW=`echo $all|perl -ne 'printf("52:54:00:00:00:%02x\n",$_)'`; arp -s 10.0.0.$all $HW;
echo $HW
done

