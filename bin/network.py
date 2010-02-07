#
# network.py - python portable port scanner
#
# Copyright 2010, savrus
# Read the COPYING file in the root of the source tree.
#

import socket
import select
import string
import errno
import sys

scan_timeout = 10

# hosts must be list of tuples (host, ip)
# returns list of tuple of up hosts
def scan_hosts(hosts):
    socks = []
    up = []
    for h in hosts:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.setblocking(0)
        err = s.connect_ex(h)
        try:
            if err == errno.EINPROGRESS:
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


def ipv4_to_int(ip):
    l = map(int,string.split(ip, '.'))
    return l[3] + l[2] * 2**8 + l[1] * 2**16 + l[0] * 2**24

def int_to_ipv4(i):
    d = i & 0xff
    c = (i & 0xff00) >> 8
    b = (i & 0xff0000) >> 16
    a = (i & 0xff000000) >> 24
    return "%s.%s.%s.%s" % (a, b, c, d)


def scan_by_range(ip_range, port):
    if string.find(ip_range, '-') == -1:
        return scan_hosts((ip_range, port))
    ip_range = string.split(ip_range, '-')
    [low, high] = map(ipv4_to_int, ip_range)
    hosts = [(int_to_ipv4(x), port) for x in range(low, high)]
    return scan_hosts(hosts)

def scan_by_mask(ip_range, port):
    if string.find(ip_range, '/') == -1:
        return scan_hosts((ip_range, port))
    ip_range = string.split(ip_range, '/')
    low = ipv4_to_int(ip_range[0])
    mask = int(ip_range[1])
    if mask > 32:
        return []
    ipmask = (0xffffffff << (32 - mask)) & 0xffffffff
    low = low & ipmask
    high = low + (1 << (32 - mask))
    hosts = [(int_to_ipv4(x), port) for x in range(low, high)]
    return scan_hosts(hosts)

def scan_host(ip, port):
    return scan_hosts([(ip, port)])

if __name__ == "__main__":
    if sys.argv[0] == "-h":
        print "Usage: %s [ip1-ip2 | ip/mask] port" % sys.argv[0]
    [net, port] = sys.argv[1:3]
    port = int(port)
    if string.find(net, '-') != -1:
        uph = scan_by_range(net, port)
    elif string.find(net, '/') != -1:
        uph = scan_by_mask(net, port)
    else:
        uph = scan_host(net, port)
    for (host, port) in uph:
        print "Host %s:%s appers to be up" % (host, port)

