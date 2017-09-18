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

trap 'trap - INT; kill -s HUP -- -$$' INT

echo -e $colorGreen Killing all processes $colorDefault
./kill_mlnx.sh

set -e

export LD_LIBRARY_PATH=.

if [ $0 != "" ]; then

    echo -e $colorGreen Flusshing Redis $colorDefault
    redis-cli flushall
#    echo -e -en "select 1\nkeys *\nset LOGLEVEL INFO" | redis-cli
fi

echo -e $colorGreen starting syncd $colorDefault
sudo LD_LIBRARY_PATH=$LD_LIBRARY_PATH ./syncd -N &
sleep 2

echo -e $colorGreen starting orchagent $colorDefault
sudo LD_LIBRARY_PATH=$LD_LIBRARY_PATH ./orchagent & # -m 00:11:11:11:11:11 &
sleep 10

echo -e $colorGreen starting portsyncd $colorDefault
sudo LD_LIBRARY_PATH=$LD_LIBRARY_PATH ./portsyncd -p sonic/platform/ACS-MSN2700/port_config.ini &
sleep 2


echo -e $colorGreen starting intfsyncd $colorDefault
sudo LD_LIBRARY_PATH=$LD_LIBRARY_PATH ./intfsyncd &
sleep 5


echo -e $colorGreen ifup $colorDefault
sudo ifup -a --force
sleep 5


echo -e $colorGreen starting neighsyncd $colorDefault
sudo LD_LIBRARY_PATH=$LD_LIBRARY_PATH ./neighsyncd &
sleep 5

# https://github.com/Azure/sonic-buildimage/blob/master/dockers/docker-orchagent/start.sh

SWSSCONFIG_ARGS="00-copp.config.json ipinip.json mirror.json "
SWSSCONFIG_ARGS+="msn2700.32ports.buffers.json msn2700.32ports.qos.json "

echo -e $colorGreen apply copp $colorDefault

for file in $SWSSCONFIG_ARGS
do
    if [ -f config.d/$file ]; then
        echo -e $colorGreen apply $file $colorDefault
        LD_LIBRARY_PATH=$LD_LIBRARY_PATH ./swssconfig config.d/$file
        sleep 1
    else
        echo -e $colorYellow file config.d/$file don\'t exists $colorDefault
    fi
done

sleep 5

echo -e $colorGreen starting fpmsyncd $colorDefault
sudo LD_LIBRARY_PATH=$LD_LIBRARY_PATH ./fpmsyncd &

echo -e $colorGreen DONE sleep $colorDefault
