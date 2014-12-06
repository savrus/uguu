#!/usr/bin/env python
#
# network.py - python portable port scanner and nameserver-related routines
#
# Copyright 2010, savrus
# Copyright (c) 2010, Radist <radist.nt@gmail.com>
# Read the COPYING file in the root of the source tree.
#

import socket
import select
import subprocess
import re
import collections
import string
import errno
import sys
import os

# online checking parameters
#connection timeout in seconds
scan_timeout = 5
#maximum simultanius connections
max_connections = 64

# DNS listing command and parse regexp
if os.name == 'nt':
    #for WinNT using nslookup
    nsls_cmd = "echo ls -t %(t)s %(d)s|nslookup - %(s)s"
    nsls_entry = "^\s(\S+)+\s+%s\s+(\S+)"
else:
    #for Unix using host
    nsls_cmd = "host -v -t %(t)s -l %(d)s %(s)s"
    nsls_entry = "^(\S+?)\.\S+\s+\d+\s+IN\s+%s\s+(\S+)"

if os.name == 'nt': nbconnect_ok = errno.WSAEWOULDBLOCK
else:               nbconnect_ok = errno.EINPROGRESS

def scan_all_hosts(hosts):
    """hosts must be list of tuples (host, ip),
returns list of tuples of up hosts"""
    res = []
    while hosts:
        res.extend(scan_hosts(hosts[0:max_connections]))
        del hosts[0:max_connections]
    return res

def scan_hosts(hosts):
    socks = []
    up = []
    for h in hosts:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.setblocking(0)
        try:
            err = s.connect_ex(h)
            if err == nbconnect_ok:
                socks.append(s)
        except:
            # catch some rare exceptions like "no such host"
            pass
    while socks:
        r_read, r_write, r_err = select.select([], socks, [], scan_timeout)
        if len(r_write) == 0:
            break
        for s in r_write:
            if s.getsockopt(socket.SOL_SOCKET, socket.SO_ERROR) == 0:
                host = s.getpeername()
                up.append(host)
        socks = list(set(socks) - set(r_write))
    return up

class dns_cache(object):
    """incorporates caching of dns records with ability to make reverse call"""
    def __init__(self):
        self.name2ip = dict()
        self.ip2name = collections.defaultdict(set)
        self.error = list()
    def __call__(self, host, ip = None):
        """usually returns ip (and caches it),
but if host is None, returns set of hostnames"""
        if ip is None:
            if host in self.name2ip:
                return self.name2ip[host]
            if host in self.error:
                return None
            try:
                ip = socket.gethostbyname(host)
            except:
                self.error.append(host)
                sys.stderr.write("Cann't resolve host name '%s'.\n" % host)
                return None
        elif host is None:
            return self.ip2name[ip]
        self.name2ip[host] = ip
        self.ip2name[ip].add(host)
        return ip

def ns_domain(domain, rtype = "A", dns = ""):
    """list rtype NS records from domain using provided or default dns,
returns dict with hostnames as keys"""
    hosts = subprocess.Popen(nsls_cmd % {'d': domain, 't': rtype, 's': dns},
                             stdin = subprocess.PIPE, stdout = subprocess.PIPE,
                             shell = True)
    re_host = re.compile(nsls_entry % rtype)
    res = dict()
    hosts.stdin.close()
    for nsentry in hosts.stdout:
        entry = re_host.search(nsentry)
        if entry is not None:
            res[entry.group(1)] = entry.group(2)
    hosts.stdout.close()
    if hosts.wait() != 0:
        sys.stderr.write("DNS zone %s listing completed with error\n" % domain)
    elif len(res) == 0:
        sys.stderr.write("DNS zone %s listing returned no records\n" % domain)
    return res

def ipv4_to_int(ip):
    l = map(int, string.split(ip, '.'))
    return l[3] + l[2] * 2 ** 8 + l[1] * 2 ** 16 + l[0] * 2 ** 24

def int_to_ipv4(i):
    d = i & 0xff
    c = (i & 0xff00) >> 8
    b = (i & 0xff0000) >> 16
    a = (i & 0xff000000) >> 24
    return "%s.%s.%s.%s" % (a, b, c, d)


def get_host_list(net, port = None):
    def from_range(ip_range):
        ip_range = string.split(ip_range, '-')
        low, high = map(ipv4_to_int, ip_range)
        return [int_to_ipv4(x) for x in range(low, high + 1)]

    def from_mask(ip_range):
        ip_range = string.split(ip_range, '/')
        low = ipv4_to_int(ip_range[0])
        mask = int(ip_range[1])
        if mask > 32:
            return []
        ipmask = (0xffffffff << (32 - mask)) & 0xffffffff
        low = low & ipmask
        high = low + (1 << (32 - mask))
        return [int_to_ipv4(x) for x in range(low, high)]

    if string.find(net, '-') != -1:
        hosts = from_range(net)
    elif string.find(net, '/') != -1:
        hosts = from_mask(net)
    else:
        hosts = [net]
    if port is not None:
        hosts = [(host, port) for host in hosts]	
    return hosts   

def scan_net(net, port):
    return scan_all_hosts(get_host_list(net, port))

def scan_host(ip, port):
    return scan_hosts([(ip, port)])

if __name__ == "__main__":
    if sys.argv[0] == "-h" or len(sys.argv) < 4:
        print "Usage:"
        print "\t%s scan {ip1-ip2 | ip/mask} port" % sys.argv[0]
        print "\t%s list domain type [nserver]" % sys.argv[0]
        sys.exit(2)
    if sys.argv[1] == "scan":
        [net, port] = sys.argv[2:4]
        port = int(port)
        uph = scan_net(net, port)
        for (host, port) in uph:
            print "Host %s:%s appers to be up" % (host, port)
    elif sys.argv[1] == "list":
        [dom, nstype] = sys.argv[2:4]
        ns = ""
        if len(sys.argv) > 4:
            ns = sys.argv[4]
        for (host, addr) in ns_domain(dom, nstype, ns).iteritems():
            print "Host %s is %s" % (host, addr)
    else:
        print "params error"
