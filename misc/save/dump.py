#!/usr/bin/env python
#
# dump.py - dump all shares' filelists
#
# Copyright 2010, savrus
# Read the COPYING file in the root of the source tree.
#

import os
import os.path
import sys
import subprocess

from common import connectdb, scanners_path, db_host, db_user, db_password, db_database, share_save_str, share_save_path

def dump_share(share, diffto = ""):
    print "Dumping %(proto)s://%(host)s:%(port)s" \
        % {'proto': share['protocol'],
           'port': str(share['port']),
           'host': share['hostname']}
    cmd = os.path.join(scanners_path, "sqlscan")
    sp = subprocess.Popen("%(cmd)s -s %(proto)s -p %(port)s -dP %(dh)s %(du)s %(dd)s %(dfile)s %(host)s > %(sfile)s"
        % {'cmd': cmd,
           'proto': share['protocol'],
           'port': str(share['port']),
           'host': share['hostname'],
           'sfile': os.path.join('dump', share_save_str(share['protocol'], share['hostname'], share['port'])),
           'dfile': "-u " + diffto if diffto != "" else "",
           'dh': "-dh " + db_host if len(db_host) > 0 else "",
           'du': "-du " + db_user if len(db_user) > 0 else "",
           'dd': "-dd " + db_database if len(db_database) > 0 else "",
           },
        shell = True, stdin=subprocess.PIPE)
    sp.communicate(input = db_password + "\n")
    sp.wait()

if __name__ == "__main__":
    try:
        db = connectdb()
    except:
        print "Unable to connect to the database, exiting."
        sys.exit()
    shares = db.cursor()
    shares.execute("""
        SELECT share_id, protocol, hostname, port FROM shares
        """)
    for share in shares.fetchall():
        if "diff" in sys.argv:
            savepath = share_save_path(share['protocol'], share['hostname'], share['port'])
            if os.path.isfile(savepath):
                dump_share(share,savepath)
                continue
        dump_share(share)

