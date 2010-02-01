#!/usr/bin/python
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
from subprocess import PIPE
from common import connectdb, nmap_cmd, nmap_online

#Default protocols' ports
default_ports = {'ftp': 21, 'smb': 139}

def make_dns_cache(db):
    cursor = db.cursor()
    cursor.execute("SELECT DISTINCT(hostname) FROM shares")
    cache = dict([(share[0], socket.gethostbyname(share[0])) for share in cursor.fetchall()])
    return cache, dict([(v, k) for (k, v) in cache.iteritems()])

def check_online_shares(hostlist, port):
    iplist = set([name2ip[host] for host in hostlist])
    nmap = subprocess.Popen(nmap_cmd % {'p': port}, shell=True,
                            stdin=PIPE, stdout=PIPE, stderr=None)
    nmap.stdin.write(string.join(iplist,"\n"))
    nmap.stdin.close()
    re_online = re.compile(nmap_online)
    online = set()
    for line in nmap.stdout:
        on = re_online.search(line)
        if not on:
            continue
        online.add(on.group(1))
    nmap.wait()
    return [ip2name[item] for item in online], [ip2name[item] for item in iplist - online]

def update_shares_state(db, selwhere, port):
    cursor = db.cursor()
    cursor.execute("SELECT share_id, hostname FROM shares WHERE %s" % selwhere)
    itemdict = dict([(row['hostname'], row['share_id']) for row in cursor.fetchall()])
    online, offline = check_online_shares(itemdict.keys(), port)
    if len(online):
        cursor.execute("UPDATE shares SET state=True  WHERE share_id IN (%s)" % \
            string.join([str(itemdict[host]) for host in online],',') )
    if len(offline):
        cursor.execute("UPDATE shares SET state=False WHERE share_id IN (%s)" % \
            string.join([str(itemdict[host]) for host in offline],',') )

def get_shares_ports(db):
    cursor = db.cursor()
    cursor.execute("SELECT DISTINCT(port) FROM shares WHERE port>0")
    return [p[0] for p in cursor.fetchall()]

try:
    db = connectdb()
except:
    print "I am unable to connect to the database, exiting."
    sys.exit()

name2ip, ip2name = make_dns_cache(db)

for proto in default_ports.iteritems():
    update_shares_state(db, "protocol='%s' AND port=0" % proto[0], proto[1])

for port in get_shares_ports(db):
    update_shares_state(db, "port=%d" % port, port)

db.commit()
