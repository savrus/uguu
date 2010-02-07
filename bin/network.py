#!/usr/bin/python
#
# network.py - python portable port scanner
#
# Copyright 2010, savrus
# Read the COPYING file in the root of the source tree.
#


import socket
import select

# hosts must be list of tuples (host, ip)
# returns list of tuple of up hosts
def scan_hosts(hosts):
    socks = []
    up = []
    for h in hosts:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.setblocking(0)
        try:
            s.connect(h)
        except:
            pass
        socks.append(s)
    while socks:
        r_read, r_write, r_err = select.select([], socks, [])
        for s in r_write:
            if s.getsockopt(socket.SOL_SOCKET, socket.SO_ERROR) == 0:
                host = s.getpeername()
                up.append(host)
        socks = list(set(socks) - set(r_write))
    return up

#example
if __name__ == "__main__":
    uph = scan_hosts([("arx.hell", 21), ("arx.hell", 8000)])

    for (host, port) in uph:
        print "Host %s:%s appers to be up" % (host, port)

