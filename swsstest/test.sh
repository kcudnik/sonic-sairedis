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
#portsyncd -p sonic/platform/ACS-MSN2700/port_config.ini &
portsyncd -p ./Force10-S6000/port_config.ini &

sleep 2

echo -e $colorGreen starting intfsyncd $colorDefault
intfsyncd &

sleep 2

#echo -e $colorGreen ifup $colorDefault
#sudo ifup -a --force
#sleep 2

# TODO first use static arp insert
echo -e $colorGreen starting neighsyncd $colorDefault
neighsyncd &

sleep 2

echo -e $colorGreen populate static ARP $colorDefault
seq 1 2 63| while read all; do HW=`echo $all|perl -ne 'printf("52:54:00:00:00:%02x\n",$_)'`; sudo arp -s 10.0.0.$all $HW; done

sleep 2

## https://github.com/Azure/sonic-buildimage/blob/master/dockers/docker-orchagent/start.sh
#
#SWSSCONFIG_ARGS="00-copp.config.json ipinip.json mirror.json "
#SWSSCONFIG_ARGS+="msn2700.32ports.buffers.json msn2700.32ports.qos.json "
#
#echo -e $colorGreen apply copp $colorDefault
#
#for file in $SWSSCONFIG_ARGS
#do
#    if [ -f config.d/$file ]; then
#        echo -e $colorGreen apply $file $colorDefault
#        LD_LIBRARY_PATH=$LD_LIBRARY_PATH ./swssconfig config.d/$file
#        sleep 1
#    else
#        echo -e $colorYellow file config.d/$file don\'t exists $colorDefault
#    fi
#done
#
#sleep 5
#
#echo -e $colorGreen starting fpmsyncd $colorDefault
#sudo LD_LIBRARY_PATH=$LD_LIBRARY_PATH ./fpmsyncd &

echo -e $colorGreen DONE sleep $colorDefault

sleep 2

 swssconfig  sample.route.json

# $ redis-cli -n 1 "keys" "*ROUTE_ENTRY:*2.2.2.0/24*"
# 1) "ASIC_STATE:SAI_OBJECT_TYPE_ROUTE_ENTRY:{\"dest\":\"2.2.2.0/24\",\"switch_id\":\"oid:0x21000000000000\",\"vr\":\"oid:0x3000000000022\"}"

#sleep 10000
