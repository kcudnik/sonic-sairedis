#!/bin/bash

# this script will build all necessary packages to build syncd inside bmv2
# docker and link agains dash libsai.so

# tested on:
# $ lsb_release -a
# No LSB modules are available.
# Distributor ID: Ubuntu
# Description:    Ubuntu 20.04.3 LTS
# Release:        20.04
# Codename:       focal

set -e

SCRIPT_DIR="$(dirname "$(readlink -f "$0")")"
SCRIPT_NAME=`basename $0`
DASH_REPO=https://github.com/sonic-net/DASH.git
DASH_DIR=DASH
DASH_COMMIT=1756b49     # latest commit on DASH that syncd was build
DASH_SAI_COMMIT=ad12e9e # origin/v1.12, since sonic-sairedis is build against this
DOCKER_JSON=/etc/docker/daemon.json
RSYSLOG_CONF=/etc/rsyslog.conf
REDIS_CONF=/etc/redis/redis.conf

SONIC_REPO_DIR=sonic
SONIC_SWSS_COMMON_REPO=https://github.com/sonic-net/sonic-swss-common.git
SONIC_SWSS_COMMON_COMMIT=5b6377c    # origin/master

SONIC_SAIREDIS_REPO=https://github.com/Azure/sonic-sairedis.git
SONIC_SAIREDIS_COMMIT=7178fb63
SONIC_SAIREDIS_DASH_TAG_METER_COMMIT=42ba825a # this patch is needed, since sairedis/SAI and DASH/SAI is not the same

SONIC_SWSS_REPO=https://github.com/sonic-net/sonic-swss.git
SONIC_SWSS_COMMIT=48a0bc67 # origin/master (before dashapi change)

SYNCD_CONTAINER_NAME_BASE=build_sonic_syncd
SYNCD_CONTAINER_NAME=build_sonic_syncd

SAI_INCLUDE_DIR=/usr/include/sai

alias log='git --no-pager log --graph --oneline --decorate --date=relative --all -n 20'
alias l='ls -alh --color'
alias ..='cd ..'
alias ...='cd ../..'

usage()
{
    echo "
Usage: $SCRIPT_NAME [-h] [-d] -l working_dir -c workind_dir

  -l working_dir   This will be working dir for this script and docker (required)
  -d               Will run this script in syncd build docker (internal command)
  -c working_dir   Clean everything in working dir, remove syncd docker
  -h               Display help
" >&2
    exit 1
}

host_update_docker_json()
{
    echo " * host update docker json with log-deriver syslog"

    if [ ! -f $DOCKER_JSON.bak ]; then
        echo " * making backup of $DOCKER_JSON"
        sudo cp $DOCKER_JSON $DOCKER_JSON.bak
    fi

    if grep -qP "log-driver.+syslog" $DOCKER_JSON; then
        echo " * $DOCKER_JSON already contains correct log-driver"
        return
    fi

    echo " * updating $DOCKER_JSON with syslog capability"

    sudo bash -c 'cat <<EOF > '$DOCKER_JSON'
{
  "log-driver": "syslog",
  "log-opts": {
    "syslog-address": "tcp://127.0.0.1:514",
    "syslog-facility": "daemon",
    "syslog-format": "rfc5424micro"
  }
}
EOF'

    echo " * update success, restarting deamons: daemon-reload, docker"

    sudo systemctl daemon-reload
    sudo systemctl restart docker

    echo " * restarting daemons success"
}

host_update_rsyslog_conf()
{
    echo " * host update $RSYSLOG_CONF"

    if [ ! $RSYSLOG_CONF.bak ]; then
        echo " * making backup of $RSYSLOG_CONF"
        sudo cp $RSYSLOG_CONF $RSYSLOG_CONF.bak
    fi

    if grep -q SONiCFileFormat $RSYSLOG_CONF; then
        echo " * $RSYSLOG_CONF already configured with SONiCFileFormat"
        return
    fi

    echo " * enable tcp and upd module in syslog"

    sudo perl -i.bk -npe 's/^#*// if /^#.+(imudp|imtcp)/' $RSYSLOG_CONF

    echo " * put syslog sonic logging template"

    sudo perl -i.bk -ne 'print if not /SONiCFileFormat|ActionFileDefaultTemplate/' $RSYSLOG_CONF

    sudo sed -ie '/GLOBAL DIRECTIVES/{s/$/\n\$template SONiCFileFormat,"%TIMESTAMP%.%timestamp:::date-subseconds% %HOSTNAME% %syslogseverity-text:::uppercase% %syslogtag%%msg:::sp-if-no-1st-sp%%msg:::drop-last-lf%\\n"\n\$ActionFileDefaultTemplate SONiCFileFormat/}' $RSYSLOG_CONF

    echo " * restarting syslog"

    sudo service rsyslog restart

    logger foobar

    echo " * restarting syslog service success"
}

host_setup_redis_server()
{
    echo " * host setup redis server"

    # in this scenario we are assuming redis server is running in host
    # environment if any changes would be needed, like rinning redis in separate
    # docker then some updates would be needed


    if [ $( dpkg --list |grep -P "\bredis-server\b" | wc -l ) -eq 0 ]; then
        echo " * installing redis-server package"
        sudo apt-get install -y redis-server
    else
        echo " * redis-server already installed"
    fi

    if [ ! -f $REDIS_CONF.bak ]; then
        echo " * making copy of $REDIS_CONF"
        sudo cp $REDIS_CONF $REDIS_CONF.bak
    fi

    if grep -q "redis.sock" $REDIS_CONF; then
        echo " * $REDIS_CONF correctly configured"
        return
    fi

    echo " * updating $REDIS_CONF"

    sudo perl -i.bk -npe 's/^# unixsocket/unixsocket/' $REDIS_CONF
    sudo perl -i.bk -npe 's/redis-server.sock/redis.sock/' $REDIS_CONF

    echo " * restarting redis-server"

    sudo service redis-server restart

    echo " * redis-server restart success"
}

host_clone_sonic_repos()
{
    echo " * host clone sonic repos"

    cd $TOP_DIR

    if [ ! -d $SONIC_REPO_DIR ]; then
        mkdir $SONIC_REPO_DIR
    fi

    cd $SONIC_REPO_DIR

    if [ ! -d sonic-swss-common ]; then
        echo " * clonning $SONIC_SWSS_COMMON_REPO"
        git clone $SONIC_SWSS_COMMON_REPO
    fi

    cd sonic-swss-common

    HEAD_COMMIT=$(git rev-parse --short HEAD)

    if [ $HEAD_COMMIT == $SONIC_SWSS_COMMON_COMMIT ]; then
        echo " * sonic-swss-common is already on $SONIC_SWSS_COMMON_COMMIT"
    else
        echo " * fetching all on sonic-swss-common"
        git fetch --all
        git checkout $SONIC_SWSS_COMMON_COMMIT
    fi

    cd ..

    if [ ! -d sonic-sairedis ]; then
        echo " * clonning $SONIC_SAIREDIS_REPO"
        git clone $SONIC_SAIREDIS_REPO
        echo " * adding remote kcudnik repo (needed for tag/meter cherry-pick)"
        cd sonic-sairedis
        git remote add kcudnik https://github.com/kcudnik/sonic-sairedis.git
        git fetch --all
        cd ..
    fi

    cd sonic-sairedis

    echo " * fetching all on sonic-sairedis"

    HEAD_COMMIT=$(git rev-parse --short HEAD)

    if [ $HEAD_COMMIT == $SONIC_SAIREDIS_COMMIT ]; then
        echo " * sonic-sairedis is already on $SONIC_SAIREDIS_COMMIT"
    elif [ $( git diff $HEAD_COMMIT $SONIC_SAIREDIS_DASH_TAG_METER_COMMIT | wc -l ) -eq 0 ]; then
        echo " * seems like sonic-sairedis tag/meter patch already applied"
    else
        echo " * fetching all on sonic-sairedis"
        git fetch --all
        git checkout $SONIC_SAIREDIS_COMMIT
    fi

    echo " * NOTICE: sairedis don't need init submodule SAI, since it will use auto generated headers from DASH"

    cd ..

    if [ ! -d sonic-swss ]; then
        echo " * clonning $SONIC_SWSS_REPO"
        git clone $SONIC_SWSS_REPO
    fi

    cd sonic-swss

    HEAD_COMMIT=$(git rev-parse --short HEAD)

    if [ $HEAD_COMMIT == $SONIC_SWSS_COMMIT ]; then
        echo " * sonic-swss already on $SONIC_SWSS_COMMIT"
    else
        echo " * fetching all on sonic-swss"
        git fetch --all
        git checkout $SONIC_SWSS_COMMIT
    fi

    cd ..
}

host_setup_bmv2_syncd_docker()
{
    echo " * host setup bmv2 syncd docker"

    cd $TOP_DIR

    if [ $( docker ps -a -f name=$SYNCD_CONTAINER_NAME | wc -l ) -eq 2 ]; then
        echo " * container $SYNCD_CONTAINER_NAME exists"
    else
        echo " * creating $SYNCD_CONTAINER_NAME container"

        docker container create \
            --log-driver syslog \
            --log-opt syslog-address=udp://127.0.0.1:514 \
            --ulimit core=-1 \
            -p 6379:6379 \
            -p 9559:9559 \
            -p 514:514 \
            -v $TOP_DIR/DASH/dash-pipeline/bmv2/:/bmv2 \
            -v $TOP_DIR/DASH/dash-pipeline/SAI:/SAI \
            -v $TOP_DIR/sonic:/root \
            -v /var/run/redis/:/run/redis \
            --network=host \
            -it \
            -w /root \
            -u root \
            --name $SYNCD_CONTAINER_NAME\
            sonicdash.azurecr.io/dash-bmv2-bldr:beeeda3f7ae \
            bash
    fi

    cd $TOP_DIR

    echo " * copy $SCRIPT_NAME to syncd container"

    docker cp -q $SCRIPT_DIR/$SCRIPT_NAME $SYNCD_CONTAINER_NAME:/root/$SCRIPT_NAME

    echo " * copy succeeded"
}

host_exec_script_in_container()
{
    echo " * host exec script in container $SYNCD_CONTAINER_NAME"

    echo " * starting docker $SYNCD_CONTAINER_NAME"

    docker start $SYNCD_CONTAINER_NAME

    docker exec -it $SYNCD_CONTAINER_NAME bash -c "cd /root && ./$SCRIPT_NAME -d"

    echo " * host exec script in container $SYNCD_CONTAINER_NAME success"
}

host_test_syncd_and_saiplayer()
{
    echo " * HOST TEST SYNCD AND SAIPLAYER"

    cd $TOP_DIR

    echo " * starting docker $SYNCD_CONTAINER_NAME"

    docker start $SYNCD_CONTAINER_NAME

    if [ $( docker exec -it $SYNCD_CONTAINER_NAME bash -c "service rsyslog status" | grep running | wc -l ) -eq 0 ]; then
        echo " * restarting rsyslog service in docker"
        docker exec -it $SYNCD_CONTAINER_NAME bash -c "service rsyslog restart"
    else
        echo " * seems like rsyslogd is running in docker already"
    fi

    # NOTE: if p4 switch was already configured then it may require restart make

    SIMPLE_SWITCH_CONTAINER="simple_switch-$USER"

    echo " * simple switch container is $SIMPLE_SWITCH_CONTAINER"

    if [ $(docker ps -q --filter "name=$SIMPLE_SWITCH_CONTAINER" --filter="status=running"| wc -l) -eq 1 ]; then
        echo " * seems like P4 simple switch is running"
    else
        echo " * ERROR: simple switch docker is not runnin, run in separate terminal: make -C DASH/dash-pipeline run-switch"
        exit 1
    fi

    if [ $(docker exec -it $SIMPLE_SWITCH_CONTAINER bash -c "ps -ef" | grep "[s]imple_switch_grpc" | wc -l) -eq 1 ]; then
        echo " * seems like simple_switch_grpc is running"
    else
        echo " * FATAL: simple_switch_grpc is not running :( FIXME"
        exit 1
    fi

    echo " * killing potentian syncd running and saiplayer running in docker"

    docker exec -it $SYNCD_CONTAINER_NAME bash -c "pkill -9 syncd; pkill -9 saiplayer;true"

    echo " * flushing all redis database"

    redis-cli FLUSHALL

    echo " * starging syncd in background in $SYNCD_CONTAINER_NAME"

    docker exec -itd $SYNCD_CONTAINER_NAME bash -c "export DASH_USE_NOT_SUPPORTED=1;syncd -SUu -z redis_async"

    sleep 1

    if [ $(docker exec -it $SYNCD_CONTAINER_NAME bash -c "ps -ef" |grep "[s]yncd" |wc -l) -eq 1 ]; then
        echo " * seems like syncd is running"
    else
        echo " * FATAL: syncd is not running in $SYNCD_CONTAINER_NAME, FIXME"
        exit 1
    fi

    cat <<EOF >foo.rec
2019-02-14.20:16:47.696163|a|INIT_VIEW
2019-02-14.20:17:09.080480|A|SAI_STATUS_SUCCESS
2019-02-14.20:17:09.083395|c|SAI_OBJECT_TYPE_SWITCH:oid:0x21000000000000|SAI_SWITCH_ATTR_INIT_SWITCH=true
2017-06-14.01:56:05.520538|g|SAI_OBJECT_TYPE_SWITCH:oid:0x21000000000000|SAI_SWITCH_ATTR_PORT_LIST=2:oid:0x0,oid:0x0
2017-06-14.01:56:05.525938|G|SAI_STATUS_SUCCESS|SAI_SWITCH_ATTR_PORT_LIST=2:oid:0x1000000000002,oid:0x1000000000003
2019-02-14.20:17:14.021109|a|APPLY_VIEW
2019-02-14.20:17:09.080480|A|SAI_STATUS_SUCCESS
EOF

    echo " * copy recording to replay"

    docker cp -q foo.rec $SYNCD_CONTAINER_NAME:/root/foo.rec

    echo " * runing simple saiplayer to test syncd communication"

    docker exec -it $SYNCD_CONTAINER_NAME saiplayer -u -r -z redis_async /root/foo.rec

#    echo " * display redis contents"
#    redis-cli -n 1 keys "*"
#    redis-cli -n 1 hgetall HIDDEN
#    redis-cli -n 1 hgetall RIDTOVID

    echo " * checking if port number is correct"

    if [ $(redis-cli -n 1 keys "*"|grep SAI_OBJECT_TYPE_PORT: | wc -l) -eq 3 ]; then
        echo " * saiplayer and syncd p4/bmv2 communication SUCCESS"
    else
        echo -n " * FATAL: wrong number of portts in redis, expected 3, got: "
        redis-cli -n 1 keys "*"|grep SAI_OBJECT_TYPE_PORT: | wc -l
        exit 1
    fi

    echo " * listing obtained ports:"

    redis-cli -n 1 keys "*"|grep SAI_OBJECT_TYPE_PORT:

    echo " * shutting down syncd"

    docker exec -it $SYNCD_CONTAINER_NAME syncd_request_shutdown -c cold

    echo " * HOST TEST SYNCD AND SAIPLAYER - SUCCESS"
}

host_main()
{
    echo " * HOST MAIN SCRIPT"

    cd $WORKING_DIR

    echo " * working directory is: '$WORKING_DIR' ($PWD)"

    TOP_DIR=$PWD

    if [ ! -f $TOP_DIR/uuid ]; then
        echo " * generate new UUID for this directory (used in container name)"
        UUID=$(uuidgen -r|cut -c1-8)
        echo $UUID > $TOP_DIR/uuid
        echo " * new uuid is $UUID"
    else
        UUID=$(cat uuid)
        echo " * read uuid from file $UUID"
    fi

    SYNCD_CONTAINER_NAME="$SYNCD_CONTAINER_NAME_BASE-$UUID"

    echo " * syncd container name is $SYNCD_CONTAINER_NAME"

    if [ ! -d $DASH_DIR ]; then

        echo "* cloning DASH repo $DASH_REPO"

        git clone $DASH_REPO
    fi

    echo " * entering $DASH_DIR"

    cd $DASH_DIR

    HEAD_COMMIT=$(git rev-parse --short HEAD)

    if [ $HEAD_COMMIT == $DASH_COMMIT ]; then
        echo " * DASH is already on $DASH_COMMIT"
    else
        echo " * checking out DASH commit $DASH_COMMIT"
        git checkout $DASH_COMMIT
    fi

    if [ ! -f dash-pipeline/SAI/SAI/Makefile ]; then
        echo " * init DASH repo submodules"
        git submodule update --init
    fi

    cd dash-pipeline/SAI/SAI

    HEAD_COMMIT=$(git rev-parse --short HEAD)

    if [ $HEAD_COMMIT == $DASH_SAI_COMMIT ]; then
        echo " * DASH SAI is already on $DASH_SAI_COMMIT"
    else
        echo " * checking out DASH SAI to $DASH_SAI_COMMIT"
        git checkout $DASH_SAI_COMMIT
    fi

    cd $TOP_DIR

    cd $DASH_DIR/dash-pipeline

    if grep -q $DASH_SAI_COMMIT Makefile; then
        echo " * Makefile already updated with SAI commit"
    else
        echo " * update dash-pipeline/Makefile to use DASH SAI commit $DASH_SAI_COMMIT on sai-clean"
        perl -i.bk -npe 'print "\tcd SAI/SAI && git checkout '$DASH_SAI_COMMIT'\n" if /inc experimental meta/' Makefile
    fi

    # this will take a while, make sai is enough to make libsai.so, but all will be required to make run-switch

    if [ ! -f SAI/lib/libsai.so ]; then
        echo " * making all in DASH/dash-pipeline (this will take a while)"
        make all
    fi

    echo " * build libsai.so succeeded"

    host_update_docker_json   # maybe not needed
    host_update_rsyslog_conf
    host_setup_redis_server
    host_clone_sonic_repos
    host_setup_bmv2_syncd_docker
    host_exec_script_in_container
    host_test_syncd_and_saiplayer

    echo " * HOST MAIN SUCCESS"

    exit 0
}

docker_install_packages()
{
    echo " * docker install packages"

    if [ -f /usr/bin/doxygen ]; then
        echo " * seems like all necessary packages already installed"
    else
        echo " * installing necessary packages"

        apt-get update
        apt-get install -y rsyslog
        apt-get install -y fakeroot dh-make dh-exec
        apt-get install -y vim git
        apt-get install -y netcat net-tools strace
        apt-get install -y libhiredis-dev libyang-dev
        apt-get install -y libhiredis0.14 libyang0.16
        apt-get install -y google-mock libgmock-dev
        apt-get install -y libnl-3-dev libnl-genl-3-dev libnl-route-3-dev libnl-nf-3-dev
        apt-get install -y libnl-route-3-200 libnl-route-3-dev libnl-cli-3-200 libnl-cli-3-dev libnl-3-dev
        apt-get install -y libzmq5 libzmq3-dev
        apt-get install -y swig4.0
        apt-get install -y python3-dev
        apt-get install -y uuid-dev
        apt-get install -y autoconf-archive
        apt-get install -y graphviz
        apt-get install -y aspell-en
        apt-get install -y libjansson4 libjansson-dev
        apt-get install -y doxygen
    fi
}

docker_update_rsyslog()
{
    echo " * docker update rsyslogd"

    if [ ! $RSYSLOG_CONF.bak ]; then

        echo " * making backup of $RSYSLOG_CONF"

        cp $RSYSLOG_CONF $RSYSLOG_CONF.bak
    fi

    if grep -q ForwardFormatInContainer $RSYSLOG_CONF; then
        echo " * seems like $RSYSLOG_CONF already updated"

        if [ $( service rsyslog status | grep running | wc -l ) -eq 0 ]; then
            echo " * restarting rsyslog service"
            service rsyslog restart
        fi
        logger "hello from $SYNCD_CONTAINER_NAME"
        return
    fi

    echo " * update rsyslog configuration"

    perl -i.bk -npe 's/(.+)/#$1/ if /imklog/' $RSYSLOG_CONF

    echo " * put syslog sonic logging template"

    perl -i.bk -ne 'print if not /"ForwardFormatInContainer"/' $RSYSLOG_CONF

    bash -c 'cat <<EOF >> '$RSYSLOG_CONF'
template (name="ForwardFormatInContainer" type="string" string="<%PRI%>%TIMESTAMP:::date-rfc3339% %HOSTNAME% %syslogtag%%msg:::sp-if-no-1st-sp%%msg%")
*.* action(type="omfwd" target="127.0.0.1" port="514" protocol="udp" Template="ForwardFormatInContainer")
EOF'

    echo " * restarting syslog"

    service rsyslog restart

    echo " * test logger from docker"

    logger "hello from $SYNCD_CONTAINER_NAME" # this should show up in host syslog
}

docker_copy_bmv2_files()
{
    echo " * docker copy bmv2 files"

    if [ ! -d /etc/dash ]; then
        mkdir /etc/dash
    fi

    # those are needed to test run syncd inside the docker

    cp -vu /bmv2/dash_pipeline.bmv2/dash_pipeline.json /etc/dash/
    cp -vu /bmv2/dash_pipeline.bmv2/dash_pipeline_p4rt.txt /etc/dash/
}

docker_compile_sonic_swss_common()
{
    echo " * compile sonic-swss-common"

    mkdir -p /usr/share/share

    if [ -f /usr/bin/swssloglevel ] && [ -f /usr/include/swss/logger.h ] && [ -f /var/run/redis/sonic-db/database_config.json ]; then
        echo " * seems like sonic-swss-common is compiled and installed"
        return
    fi

    cd sonic-swss-common

    echo " * disable sonic-swss-common pyext and tests for compilation"

    if grep -q "pyext.+Makefile" Makefile.am; then
        sed -ie '/pyext.Makefile.am/d' Makefile.am
        sed -ie '/tests.Makefile.am/d' Makefile.am
    fi

    echo " * perform autogen and configure"

    if [ ! -f ./configure ]; then
        echo " * perform autogen"
        ./autogen.sh
    fi

    if [ ./configure -nt ./config.log ]; then
        echo " * perform configure"
        ./configure --enable-python2=no
    fi

# since building deb packages gives some errors right now, we will install swss-common manually

    echo " * performing make and make install"

    make
    make install

    echo " * installing sonic-swss-common artifacats"

    mkdir -p /usr/include/swss
    mkdir -p /usr/share/swss

    cp -vu common/*.lua /usr/share/swss/
    cp -vu common/*.hpp /usr/include/swss/
    cp -vu common/*.h /usr/include/swss/

    cd ..

    echo " * build and install sonic-swss-common succeeded"

# NOTE: to make work packages in the docker a patch must be added to debian/rules:
#
# ---
# override_dh_shlibdeps:
#     dh_shlibdeps --dpkg-shlibdeps-params=--ignore-missing-info
# ---
#
# or some docker resolution
# fakeroot debian/rules DEB_BUILD_PROFILES=nopython2 DEB_CONFIGURE_EXTRA_FLAGS='--enable-python2=no' binary
# dpkg-buildpackage -us -uc -b
#
# dh_shlibdeps
# dpkg-shlibdeps: error: no dependency information found for /usr/local/lib/libyang.so.0.16 (used by debian/libswsscommon/usr/lib/x86_64-linux-gnu/libswsscommon.so.0.0.0)

}

docker_compile_sonic_sairedis()
{
    echo " * compile sonic-sairedis"

    if [ -d /var/log/sai_failure_dump/ ] && [ -f /usr/include/sai/sairedis.h ]; then
        echo " * seems like sonic-sairedis is compiled and installed"
        return
    fi

    cd sonic-sairedis

    # no need for submodule init

    echo " * copy SAI files from generated DASH SAI"

    mkdir -p SAI

    cp -rvu /SAI/SAI/inc/ /SAI/SAI/experimental/ /SAI/SAI/meta/ SAI/

    echo " * install libsai.so from DASH" # TODO make this as deb package with all the headers

    cp -vu /SAI/lib/libsai.so /usr/local/lib/

    echo " * cherrypick DASH tag meter fix from kcudnik branch: https://github.com/kcudnik/sonic-sairedis/commit/$SONIC_SAIREDIS_DASH_TAG_METER_COMMIT"

    git config --global --add safe.directory /root/sonic-sairedis
    git config --global user.email "dashtest@example.com"
    git config --global user.name "Dash test"

    # this commit will contain all necessary changes to tag and meter

    if [ ! -f lib/sai_redis_dash_tag.cpp ]; then
        git cherry-pick $SONIC_SAIREDIS_DASH_TAG_METER_COMMIT
    fi

    echo " * disable tests and pyext on sairedis"

    if grep -q pyext Makefile.am; then
        perl -i.bk -pe 's/pyext//' Makefile.am
        perl -i.bk -pe 's/unittest//' Makefile.am
        perl -i.bk -pe 's/tests//' Makefile.am
    fi

    if grep -q pyext configure.am; then
        perl -i.bk -pe 's/pyext.+Makefile//' configure.ac
        sed -ie '/unittest/d' configure.ac
    fi

    if [ ! -f ./configure ]; then
        ./autogen.sh
    fi

    if [ ./configure -nt ./config.log ]; then
        ./configure --enable-python2=no
    fi

    if [ /usr/local/lib/libsai.so -nt ./configure ]; then
        ./configure --enable-python2=no
    fi

    make
    make install

    echo " * install sonic-sairedis artifacts"

    if [ ! -d $SAI_INCLUDE_DIR ]; then
        mkdir $SAI_INCLUDE_DIR
    fi

    cp -vu SAI/inc/sai*.h SAI/experimental/sai*.h SAI/meta/sai*.h $SAI_INCLUDE_DIR/
    cp -vu lib/sairedis.h $SAI_INCLUDE_DIR/
    cp -vu meta/sai*.h meta/Sai*.h $SAI_INCLUDE_DIR/

    cp -vu syncd/scripts/sai_failure_dump.sh /usr/bin/sai_failure_dump.sh
    chmod 755 /usr/bin/sai_failure_dump.sh

    if [ ! -d /var/log/sai_failure_dump/ ]; then
        mkdir /var/log/sai_failure_dump/
    fi

    cd ..

    echo " * build and install sonci-sairedis success"
}

docker_compile_sonic_swss()
{
    echo " * compile sonic-swss"

    if [ -f /usr/local/bin/orchagent ]; then
        echo " * seems like sonic-swss is compiled and installed" 
        return
    fi

    cd sonic-swss

    if [ ! -f ./configure ]; then
        ./autogen.sh
    fi

# this is due old libnl-route-3 in version 3.5.0, 3.6.0 is needed, we disable fpmsyncd and tests
#
# routesync.cpp: In member function 'void swss::RouteSync::getNextHopList(rtnl_route*, std::string&, std::string&, std::string&)':
# routesync.cpp:1130:25: error: 'rtnl_route_nh_get_encap_mpls_dst' was not declared in this scope; did you mean 'rtnl_route_nh_encap_mpls'?
#  1130 |             if ((addr = rtnl_route_nh_get_encap_mpls_dst(nexthop)))

    if grep -q fpmsyncd ./configure.ac; then
        echo " * removing fpmsyncd and tests from make"
        sed -ie '/fpmsyncd/d' configure.ac
        perl -i.bk -npe 's/fpmsyncd//g' Makefile.am
        perl -i.bk -npe 's/tests//g' Makefile.am
    fi

    if [ ./configure -nt ./config.log ]; then
        ./configure
    fi

    make
    make install

    cd ..

    echo " * build and install of sonic-swss succeeded"
}

docker_main()
{
    echo " * DOCKER MAIN SCRIPT"

    docker_install_packages
    docker_update_rsyslog
    docker_copy_bmv2_files

    docker_compile_sonic_swss_common
    docker_compile_sonic_sairedis
    docker_compile_sonic_swss

    echo " * DOCKER MAIN SUCCESS"

    exit 0
}

host_clean()
{
    echo " * HOST CLEAN"

    set -e

    cd $WORKING_DIR

    echo " * working directory is: '$WORKING_DIR' ($PWD)"

    TOP_DIR=$PWD

    cd $TOP_DIR

    if [ ! -f uuid ]; then
        echo " * ERROR: uuid file is required in workind directory to clean"
        exit 1
    fi

    UUID=$(cat uuid)

    SYNCD_CONTAINER_NAME="$SYNCD_CONTAINER_NAME_BASE-$UUID"

    echo " * stopping and removind syncd container $SYNCD_CONTAINER_NAME"

    exit 1

    docker stop $SYNCD_CONTAINER_NAME || true
    docker remove $SYNCD_CONTAINER_NAME || true

    echo " * removing $DASH_DIR $SONIC_REPO_DIR foo.rec uuid"

    sudo rm -rf "$DASH_DIR"
    sudo rm -rf "$SONIC_REPO_DIR"
    rm -f foo.rec
    rm -f uuid

    echo " * HOST CLEAN SUCCESS"

    exit 0
}

#############
### MAIN
#############

WORKING_DIR="."

while getopts "hdl:c:" o;
do
    case "${o}" in
        l)
            WORKING_DIR=${OPTARG}

            if [ ! -d $WORKING_DIR ] || [ "$WORKING_DIR" == "" ];
            then
                echo Directory "'$WORKING_DIR'" do not exist, required
                exit 1
            fi

            host_main
            exit 0
            ;;
        d)
            docker_main
            exit 0
            ;;
        c)
            WORKING_DIR=${OPTARG}

            if [ ! -d $WORKING_DIR ] || [ "$WORKING_DIR" == "" ];
            then
                echo Directory "'$WORKING_DIR'" do not exist, required
                exit 1
            fi

            host_clean
            exit 0
            ;;
        h)
            usage
            ;;
        *)
            usage
            ;;
    esac
done

shift $((OPTIND-1))

usage
