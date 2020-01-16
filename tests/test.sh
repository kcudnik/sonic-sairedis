#!/bin/bash

HOMEDIR=/home/sonic
SYNCD=${HOMEDIR}/sonic-sairedis/tests/vssyncd
SAIPLAYER=${HOMEDIR}/sonic-sairedis/saiplayer/saiplayer
REQUEST_SHUTDOWN=${HOMEDIR}/sonic-sairedis/syncd/syncd_request_shutdown

LAST_LINES=100000

colorDefault="\033[01;00m";
colorGreenBlue="\033[104;92m";
colorBlackYellow="\033[103;30m";
colorBlackRed="\033[30;48m";
colorRed2="\033[6;38m";
colorRed="\033[66;91m";
colorGreen="\033[66;92m";
colorYellow="\033[66;93m";
colorBlue="\033[66;94m";
colorAqua="\033[66;96m";


function kill_syncd()
{
    echo "kill syncd"
    killall -9 vssyncd syncd lt-vssyncd lt-syncd 2>/dev/null
    ps -ef | grep [s]yncd
}

function kill_saiplayer()
{
    echo "kill saiplayer"
    killall -9 saiplayer lt-saiplayer 2>/dev/null
    ps -ef | grep [s]aiplayer
}

function flush_redis() 
{
    echo "flush redis"
    redis-cli FLUSHALL 
}

generate_id()
{
    ID=`dd if=/dev/urandom count=1 2>/dev/null |md5sum|awk '{print $1}'`

    echo ID = $ID
    logger ":- $ID (start)"
}

function remove_all_namesapces()
{
    echo remove all namespaces 

    #ip -all netns delete
    ip netns list |awk '{print $1}'| while read all; do
    ip netns delete "$all"
    done
}

function setup_servers()
{
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

    ip link add srv1eth0 type veth peer name vEthernet0
    ip link add srv2eth0 type veth peer name vEthernet4

    ip link set srv1eth0 netns srv1
    ip link set srv2eth0 netns srv2
    ip link set vEthernet0 netns sw1
    ip link set vEthernet4 netns sw1

    echo routes

    ip netns exec srv1 ip a a 10.1.0.1/24 dev srv1eth0
    ip netns exec srv1 ip l s dev srv1eth0 up
    ip netns exec srv1 route add default gw 10.1.0.2

    ip netns exec srv2 ip a a 10.2.0.1/24 dev srv2eth0
    ip netns exec srv2 ip l s dev srv2eth0 up
    ip netns exec srv2 route add default gw 10.2.0.2
}

function show_address
{
    ip netns exec srv1 ip a s
    ip netns exec srv1 route -n

    ip netns exec srv2 ip a s
    ip netns exec srv2 route -n

    ip netns exec sw1 ip a s
    ip netns exec sw1 route -n
}

function show_route()
{
    echo
    echo route for SRV 1
    echo =======================
    ip netns exec srv1 route -n

    echo
    echo route for SRV 2
    echo ======================
    ip netns exec srv2 route -n

    echo
    echo route for SWITCH
    echo ========================
    ip netns exec sw1 route -n
}

function show_arp()
{
    echo
    echo arp for SRV 1
    echo =======================
    ip netns exec srv1 arp -n

    echo
    echo arp for SRV 2
    echo ======================
    ip netns exec srv2 arp -n

    echo
    echo arp for SWITCH
    echo ========================
    ip netns exec sw1 arp -n
}

function show_route_arp()
{
    echo
    echo route and arp for SRV 1
    echo =======================
    #ip netns exec srv1 ip a s
    ip netns exec srv1 route -n
    ip netns exec srv1 arp -n

    echo
    echo route and arp for SRV 2
    echo ======================
    #ip netns exec srv2 ip a s
    ip netns exec srv2 route -n
    ip netns exec srv2 arp -n

    echo
    echo route and arp for SWITCH
    echo ========================
    #ip netns exec sw1 ip a s
    ip netns exec sw1 route -n
    ip netns exec sw1 arp -n
}

function wait_for_log()
{
    echo waiting for "$1 on $ID"
    echo -en $colorBlue
    timeout 10 bash -c "tail -f /var/log/all -n $LAST_LINES | grep $ID -A ${LAST_LINES} --line-buffered |grep -P \"$1\" --line-buffered -m 1"
    echo -en $colorDefault
}

function syncd()
{
    echo running vssyncd

    timeout 15 ip netns exec sw1 $SYNCD -SUu -p $1 &
    PID_syncd=$!
    sleep 2
    #wait_for_log "starting main loop"
}

function test_ping()
{
    echo ping

    echo is switch reachable from srv 1
    ip netns exec srv1 ping -c 1 10.1.0.1 >/dev/null
    ip netns exec srv1 ping -c 1 10.1.0.2 >/dev/null

    echo is switch reachable from srv 2
    ip netns exec srv2 ping -c 1 10.2.0.1 >/dev/null
    ip netns exec srv2 ping -c 1 10.2.0.2 >/dev/null

    echo is srv 2 reachable from srv 1 
    ip netns exec srv1 ping -c 1 10.2.0.1 >/dev/null
    ip netns exec srv2 ping -c 1 10.1.0.1 >/dev/null

    echo " ping OK "
}

function setup_interfaces()
{
    echo "setup interfaces"

    ip netns exec sw1 ip a a 10.1.0.2/24 dev Ethernet0
    ip netns exec sw1 ip l s dev Ethernet0 up

    ip netns exec sw1 ip a a 10.2.0.2/24 dev Ethernet4
    ip netns exec sw1 ip l s dev Ethernet4 up

    ip netns exec sw1 ip l s dev vEthernet0 up
    ip netns exec sw1 ip l s dev vEthernet4 up
}

function player()
{
    echo running player
    timeout 10 ip netns exec sw1 $SAIPLAYER $1 &
    PID_saiplayer=$!
    sleep 1
}

function add_route()
{
    # route will be automatically added ? how
    #ip netns exec sw1 route add -net 10.1.0.0/24 dev Ethernet0
    #ip netns exec sw1 route add -net 10.2.0.0/24 dev Ethernet4

    #ip netns exec sw1 ip a s
    #ip netns list
    echo 
}

function wait_player()
{
    echo waiting for saiplayer
    wait $PID_saiplayer
}

function shutdown_syncd()
{
    echo "request syncd shutdown"
    $REQUEST_SHUTDOWN -c
}

function wait_syncd()
{
    echo waiting for syncd
    wait $PID_syncd
}

function setup_trap()
{
    trap "echo -e '\033[66;91m -- FAIL -- \033[01;00m' " ERR
    set -eE

    # any command in pipe fail
    set -o pipefail
}

function clear_trap()
{
    # clear  traps
    set +e
    trap - ERR
    trap
}

function success()
{
    clear_trap;
    echo -e "$colorGreen -- SUCCESS -- $colorDefault"
    echo

    logger ":- $ID (end)"
}

function fail()
{
    echo -e "$colorRed2 -- FAIL: $1 -- $colorDefault"
    exit 1;
}

function join_by { result=`perl -e '$s=shift@ARGV;print join($s,@ARGV)' "$@"`; }

function assert_log()
{
    echo "assert log: '$1'"
    echo -en $colorBlue
    cat /var/log/all |grep $ID -A ${LAST_LINES}|grep -P "$1"
    echo -en $colorDefault
}

function assert_not_log()
{
    echo "assert log: '$1'"
    echo -en $colorBlue
    cat /var/log/all |grep $ID -A ${LAST_LINES}|perl -ne 'BEGIN{$e=0} if (/'"$1"'/){print; $e=1}END{exit $e}'
    echo -en $colorDefault
}

function assert_log_order()
{
    join_by "|" "$@"
    str=$result;

    join_by ".*\\n.*" "$@"
    per=$result

    join_by " -> " "$@"
    str2=$result

    echo "assert log order: '($str2)'"
    #echo "assert log order: '($str)' -> ($per)"
    echo -en $colorBlue
    #cat /var/log/all |tail -n ${LAST_LINES}|grep -P "($str)" | \
    cat /var/log/all |grep $ID -A ${LAST_LINES}|grep -P "($str)" | \
        perl -e 'my $s = do { local $/; <> }; print $s;exit 1 if not $s =~ /'"$per"'/'
    echo -en $colorDefault
}

function test_name
{
    echo -e "$colorAqua *** $@ *** $colorDefault"
}

function redis_contains
{
    echo redis contains $1
    echo -en $colorBlue
    redis-cli -n 1 keys "*$1*" | grep "$1"
    echo -en $colorDefault
}

function redis_missing
{
    echo redis missing "$1"
    echo -en $colorBlue
    redis-cli -n 1 keys "*$1*" | grep -v "$1"
    echo -en $colorDefault
}

# TODO we could have methods with timouts that watch logs for
# * syncd start
# * syncd  apply view
# * player apply view
# 
# this way we can get rid of sleeps

##############################
#           TESTS
##############################

function test_server_ping()
{
    test_name ${FUNCNAME[0]}

    kill_syncd;
    kill_saiplayer;
    setup_trap;
    flush_redis;
    generate_id;
    remove_all_namesapces;
    setup_servers;
    #show_address;

    syncd ${HOMEDIR}/tests2/vsprofile.ini
    player ${HOMEDIR}/tests2/test_server_ping.rec

    setup_interfaces;
    test_ping
    wait_player
    shutdown_syncd -c
    wait_syncd
    success;
}

function test_exit_without_rm_hostif()
{
    test_name ${FUNCNAME[0]}

    kill_syncd;
    kill_saiplayer;
    setup_trap;
    flush_redis;
    generate_id;
    remove_all_namesapces;
    setup_servers;
    #show_address;

    syncd ${HOMEDIR}/tests2/vsprofile.ini
    player ${HOMEDIR}/tests2/test_exit_without_rm_hostif.rec

    setup_interfaces;
    test_ping
    wait_player
    shutdown_syncd -c
    wait_syncd
    success;
}

function test_rm_hostif()
{
    test_name ${FUNCNAME[0]}

    kill_syncd;
    kill_saiplayer;
    setup_trap;
    flush_redis;
    generate_id;
    remove_all_namesapces;
    setup_servers;
    #show_address;

    syncd ${HOMEDIR}/tests2/vsprofile.ini
    player ${HOMEDIR}/tests2/test_rm_hostif.rec

    setup_interfaces;
    test_ping
    # show_route_arp;
    wait_player
    shutdown_syncd -c
    wait_syncd

    assert_log "joined threads for hostif: Ethernet0"
    assert_log "joined threads for hostif: Ethernet4"

    # destroy must be explicit after shutdown request
    assert_log_order "joined threads for hostif: Ethernet" "received COLD switch shutdown event"
    success;
}

function test_lag()
{
    test_name ${FUNCNAME[0]}

    kill_syncd;
    kill_saiplayer;
    setup_trap;
    flush_redis;
    generate_id;
    remove_all_namesapces;
    setup_servers;
    #show_address;

    syncd ${HOMEDIR}/tests2/vsprofile.ini
    player ${HOMEDIR}/tests2/test_lag.rec

    setup_interfaces;
    test_ping
    # show_route_arp;
    wait_player
    shutdown_syncd -c
    wait_syncd

    assert_log ": newlink:.+ifname: vEthernet4"
    assert_log ": newlink:.+ifname: vEthernet0"

    success;
}

function test_explicit_port_up_ntf()
{
    test_name ${FUNCNAME[0]}

    kill_syncd;
    kill_saiplayer;
    setup_trap;
    flush_redis;
    generate_id;
    remove_all_namesapces;
    setup_servers;
    #show_address;

    syncd ${HOMEDIR}/tests2/vsprofile.ini
    player ${HOMEDIR}/tests2/test_explicit_port_up_ntf.rec

    setup_interfaces;
    test_ping
    test_ping
    # show_route_arp;
    wait_player
    shutdown_syncd -c
    wait_syncd

    # leave interfaces

    flush_redis;
    generate_id;

    syncd ${HOMEDIR}/tests2/vsprofile.ini
    player ${HOMEDIR}/tests2/test_explicit_port_up_ntf.rec

    sleep 1

    wait_player
    shutdown_syncd -c
    wait_syncd

    assert_log "OTIFY for port oid:0x100000001: SAI_PORT_OPER_STATUS_UP .port was UP."
    assert_log "OTIFY for port oid:0x100000002: SAI_PORT_OPER_STATUS_UP .port was UP."
    assert_log "joined threads for hostif: Ethernet0"

    success;
}

function test_fdb_flush()
{
    test_name ${FUNCNAME[0]}

    kill_syncd
    kill_saiplayer
    setup_trap
    flush_redis
    generate_id
    remove_all_namesapces
    setup_servers
    #show_address;

    syncd ${HOMEDIR}/tests2/vsprofile.ini
    player ${HOMEDIR}/tests2/test_fdb_flush.rec

    setup_interfaces;
    test_ping

    redis_contains "ASIC_STATE:SAI_OBJECT_TYPE_FDB_ENTRY"

    # show_route_arp;
    wait_player

    redis_missing "ASIC_STATE:SAI_OBJECT_TYPE_FDB_ENTRY"

    shutdown_syncd -c
    wait_syncd
    success;
}

function test_fdb_aging()
{
    test_name ${FUNCNAME[0]}

    kill_syncd
    kill_saiplayer
    setup_trap
    flush_redis
    generate_id
    remove_all_namesapces
    setup_servers
    #show_address;

    syncd ${HOMEDIR}/tests2/vsprofile.ini
    player ${HOMEDIR}/tests2/test_fdb_aging.rec

    setup_interfaces;
    test_ping

    redis_contains "ASIC_STATE:SAI_OBJECT_TYPE_FDB_ENTRY"

    wait_player

    sleep 4;

    redis_missing "ASIC_STATE:SAI_OBJECT_TYPE_FDB_ENTRY"

    shutdown_syncd -c
    wait_syncd
    success;
}


function test_rif_not_removed()
{
    test_name ${FUNCNAME[0]}

    kill_syncd
    kill_saiplayer
    setup_trap
    flush_redis
    generate_id
    remove_all_namesapces
    setup_servers
    #show_address;

    syncd ${HOMEDIR}/tests2/vsprofile.ini
    player ${HOMEDIR}/tests2/test_rif_not_removed.rec

    setup_interfaces;
    test_ping

    wait_player
    shutdown_syncd -c
    wait_syncd
    assert_not_log "api not initialized"
    success;
}

##############################
#           MAIN
##############################

# TODO - deadlock !!! solve ! (on previous too!)
test_lag;
test_rm_hostif;
#test_rm_hostif;
#test_rm_hostif;
#test_rm_hostif;
#test_rm_hostif;
#test_rm_hostif;
#test_rm_hostif;
#test_rm_hostif;
#test_rm_hostif;
#test_rm_hostif;
#test_rm_hostif;
#test_rm_hostif;
#test_rm_hostif;
test_exit_without_rm_hostif;
test_server_ping;
test_rif_not_removed;
test_fdb_aging;
test_fdb_flush;
test_explicit_port_up_ntf;

#  TODO we need some synchronous API on player, dumm, but something that will
#  confirm that syncd was reached and previous entries were flushed to producer
# TODO - filter on fdb processing messsages
#test_fdb_lag_with_rif;
#test_fdb_lag_no_rif;

