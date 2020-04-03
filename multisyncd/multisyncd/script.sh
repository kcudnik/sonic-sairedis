#!/bin/bash

killall -9 vssyncd syncd lt-vssyncd  lt-syncd


set -e

redis-cli FLUSHALL

echo starting syncd GUID 0

../tests/vssyncd -SUu -p ./vsprofile.ini  -g 0 -x ./context_config.json &

echo starting syncd GUID 1

../tests/vssyncd -SUu -p ./vsprofile.ini  -g 1 -x ./context_config.json &

make 

./main

sleep 1000

