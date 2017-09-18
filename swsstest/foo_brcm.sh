#!/bin/bash

# /etc/syncd.d/dell_s6000.profile
# /etc/bcm/td2-s6000-32x40G.config.bcm
# /etc/ssw/ACS-S6000/port_config.ini
# /host/machine.conf -> /etc/machine.conf

#mknod /dev/linux-bcm-knet c 122 0
#mknod /dev/linux-uk-proxy c 125 0
#mknod /dev/linux-user-bde c 126 0
#mknod /dev/linux-kernel-bde c 127 0


sudo killall syncd
sudo killall orchagent
sudo killall portsyncd
sudo killall intfsyncd
sudo killall neighsyncd
sudo killall fpmsynd

set -e
export LD_LIBRARY_PATH=.

if [ $0 != "" ]; then

    echo flusing redis
    redis-cli flushall 
fi

echo starting syncd
#sudo LD_LIBRARY_PATH=$LD_LIBRARY_PATH strace ./syncd -N -p /etc/syncd.d/dell_s6000.profile 2>strace.syncd.log &
sudo LD_LIBRARY_PATH=$LD_LIBRARY_PATH ./syncd -N -p /etc/syncd.d/dell_s6000.profile &

sleep 2
echo starting orch
sudo LD_LIBRARY_PATH=$LD_LIBRARY_PATH ./orchagent -m 00:11:11:11:11:11 &

sleep 20

# fix traps fo arp and ip2me
#sudo LD_LIBRARY_PATH=$LD_LIBRARY_PATH ./bcmcmd "fp action add 9 GpCopyToCpu 0 0"
#sudo LD_LIBRARY_PATH=$LD_LIBRARY_PATH ./bcmcmd "fp action add 0x10 GpCopyToCpu 0 0"
# fp entry reinstall 9
# fp entry reinstall 0x10

echo starting portsyncd
sudo LD_LIBRARY_PATH=$LD_LIBRARY_PATH ./portsyncd -p /etc/ssw/ACS-S6000/port_config.ini &

#sleep 5
#echo ifup
#sudo ifup -a --force

sleep 5
echo starting interfsyncd
sudo LD_LIBRARY_PATH=$LD_LIBRARY_PATH ./intfsyncd &

sleep 5
echo starting neighsyncd
sudo LD_LIBRARY_PATH=$LD_LIBRARY_PATH ./neighsyncd &

SWSSCONFIG_ARGS="00-copp.config.json "
SWSSCONFIG_ARGS+="td2.32ports.qos.1.json td2.32ports.qos.2.json td2.32ports.qos.3.json td2.32ports.qos.4.json td2.32ports.qos.5.json td2.32ports.qos.6.json "
SWSSCONFIG_ARGS+="td2.32ports.buffers.1.json td2.32ports.buffers.2.json td2.32ports.buffers.3.json "

sleep 5

for file in $SWSSCONFIG_ARGS
do
    echo apply $file
LD_LIBRARY_PATH=$LD_LIBRARY_PATH ./swssconfig ./etc/swss/config.d/$file; 
sleep 1
done


sleep 5
echo starting fpmsync
sudo LD_LIBRARY_PATH=$LD_LIBRARY_PATH ./fpmsyncd &

echo DONE, sleep

sleep 10000
