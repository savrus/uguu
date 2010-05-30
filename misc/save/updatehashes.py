#!/usr/bin/env python
#
# hashupdate.py - update all shares' hashes of saves in trees table
#
# Copyright 2010, savrus
# Copyright 2010, Radist <radist.nt@gmail.com>
# Read the COPYING file in the root of the source tree.
#

import os
import os.path
import hashlib
import subprocess

from common import connectdb, scanners_path, db_host, db_user, db_password, db_database, share_save_path

def update_share(share, cursor):
    print "Updating hash for %(proto)s://%(host)s:%(port)s" \
        % {'proto': share['protocol'],
           'port': str(share['port']),
           'host': share['hostname']}
    try:
        hash = hashlib.md5()
        for line in open(share_save_path(share['protocol'], share['hostname'], share['port']), "rt"):
            hash.update(line)
    except:
        cursor.execute("UPDATE trees SET hash='' WHERE tree_id=%(i)s", {'i': share['tree_id']})
        return
    cursor.execute("UPDATE trees SET hash=%(h)s WHERE tree_id=%(i)s", {'h': hash.hexdigest(), 'i': share['tree_id']})

if __name__ == "__main__":
    try:
        db = connectdb()
    except:
        print "Unable to connect to the database, exiting."
        sys.exit()
    shares = db.cursor()
    shares.execute("""
        SELECT tree_id, protocol, hostname, port FROM shares
        """)
    for share in shares.fetchall():
        update_share(share, shares)
    db.commit()


