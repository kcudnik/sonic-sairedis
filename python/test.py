#!/usr/bin/python

from sairedis import *

args = dict()
args["SAI_SWITCH_ATTR_INIT_SWITCH"] = "false"

print args
r = create_switch(args);
print r

swid = r["oid"]

args = dict()
args["SAI_VLAN_ATTR_VLAN_ID"] = "9"
print args
r = create_vlan(swid, args)
print r

remove_vlan(r["oid"])
