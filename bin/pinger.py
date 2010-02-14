#!/usr/bin/env python
#
# pinger.py - online checker
#
# Copyright (c) 2010, Radist <radist.nt@gmail.com>
# Read the COPYING file in the root of the source tree.
#

import psycopg2
import sys
import socket
import subprocess
import string
import re
from network import dns_cache, scan_all_hosts
from common import connectdb, log, default_ports

def get_names_list(ips):
    res = set()
    for ip in ips:
        res|=nscache(None, ip)
    return res

def check_online_shares(hostlist, port):
    iplist = frozenset([nscache(host) for host in hostlist])
    hostlist = frozenset(hostlist)
    online = set(ip for (ip, port) in scan_all_hosts([(ip, port) for ip in iplist]))
    return get_names_list(online) & hostlist, \
           get_names_list(iplist - online) & hostlist

def update_shares_state(db, selwhere, port):
    cursor = db.cursor()
    cursor.execute("SELECT share_id, hostname FROM shares WHERE %s" % selwhere)
    itemdict = dict([(row['hostname'], row['share_id']) for row in cursor.fetchall()])
    if len(itemdict) == 0:
        return
    online, offline = check_online_shares(itemdict.keys(), port)
    if len(online):
        cursor.execute("UPDATE shares SET state='online' WHERE share_id IN %s", \
        	(tuple(itemdict[host] for host in online),))
    if len(offline):
        cursor.execute("UPDATE shares SET state='offline' WHERE share_id IN %s", \
        	(tuple(itemdict[host] for host in offline),))
    return len(online), len(offline)

def get_shares_ports(db):
    cursor = db.cursor()
    cursor.execute("SELECT DISTINCT(port) FROM shares WHERE port>0")
    return [p[0] for p in cursor.fetchall()]

if __name__ == "__main__":
    try:
        db = connectdb()
    except:
        print "I am unable to connect to the database, exiting."
        sys.exit()

    nscache = dns_cache()

    log("Starting pinger...")
    shares = 0
    online = 0

    for proto in default_ports.iteritems():
        on, off = update_shares_state(db, "protocol='%s' AND port=0" % proto[0], proto[1])
        shares += on + off
        online += on

    for port in get_shares_ports(db):
        on, off = update_shares_state(db, "port=%d" % port, port)
        shares += on + off
        online += on

    db.commit()
    log("Updated state for %4d shares, %4d are online.", (shares, online))

