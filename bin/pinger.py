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
import datetime
import psycopg2.extensions
from network import dns_cache, scan_all_hosts
from common import connectdb, log, default_ports

# max hosts in state-update query
max_update_query_hosts = 15

def get_names_list(ips):
    res = set()
    for ip in ips:
        if ip is not None:
            res|=nscache(None, ip)
    return res

def check_online_shares(hostlist, port):
    iplist = frozenset([nscache(host) for host in hostlist])
    hostlist = frozenset(hostlist)
    online = set(ip for (ip, port) in scan_all_hosts([(ip, port) for ip in iplist]))
    online = get_names_list(online) & hostlist
    return online, hostlist - online

def set_shares_state(cursor, hostlist, state):
    while len(hostlist):
        cursor.execute("""
            UPDATE shares SET state='%s',hostaddr=inet(sharelist.column2)
            FROM (VALUES %s) sharelist WHERE id=sharelist.column1
            """ % (
                'online' if state else 'offline',
                cursor.mogrify('%s', (tuple(hostlist[:max_update_query_hosts]),))[1:-1]
                ))
        del hostlist[:max_update_query_hosts]

def update_shares_state(db, selwhere, port):
    cursor = db.cursor()
    cursor.execute("SELECT id, hostname FROM shares WHERE %s" % selwhere)
    itemdict = dict((row['hostname'], [row['id'],  nscache(row['hostname'])]) for row in cursor.fetchall())
    if len(itemdict) == 0:
        return
    online, offline = check_online_shares(itemdict.keys(), port)
    set_shares_state(cursor, [itemdict[host] for host in online], True)
    set_shares_state(cursor, [itemdict[host] for host in offline], False)
    return len(online), len(offline)

def get_shares_ports(db):
    cursor = db.cursor()
    cursor.execute("SELECT DISTINCT(port) FROM shares WHERE port>0")
    return [p[0] for p in cursor.fetchall()]

# psycopg2 adapter for list object, surrounds object with parenthesis
# this is required becaouse psycopg2's tuple adapter fails with None 
class list_adapter(object):
    def __init__(self, l):
        self.__list = l
        self.__str = None
    def prepare(self, conn):
        adapts = [psycopg2.extensions.adapt(it) for it in self.__list]
        for it in adapts:
            if hasattr(it, 'prepare'): it.prepare(conn)
        self.__str = '(' + ', '.join([str(it) for it in adapts]) + ')'
    def __str__(self):
        if self.__str is None: self.prepare(db)
        return self.__str
    def getquoted(self):
        return str(self)
class None_adapter(object):
    def __init__(self,obj):
        pass
    def __str__(self):
        return 'NULL'
    def getquoted(self):
        return 'NULL'

psycopg2.extensions.register_adapter(list, list_adapter)
psycopg2.extensions.register_adapter(type(None), None_adapter)

if __name__ == "__main__":
    try:
        db = connectdb()
    except:
        print "I am unable to connect to the database, exiting."
        sys.exit()
    db.set_isolation_level(psycopg2.extensions.ISOLATION_LEVEL_AUTOCOMMIT)

    nscache = dns_cache()

    log("Starting pinger...")
    shares = 0
    online = 0
    start = datetime.datetime.now()

    for proto in default_ports.iteritems():
        on, off = update_shares_state(db, "protocol='%s' AND port=0" % proto[0], proto[1])
        shares += on + off
        online += on

    for port in get_shares_ports(db):
        on, off = update_shares_state(db, "port=%d" % port, port)
        shares += on + off
        online += on

    log("Updated state for %4d shares, %4d are online (running time %s).", (shares, online, datetime.datetime.now() - start))

