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

shares_dump_dir = 'dump'
shares_dump_dir = os.path.join(os.path.dirname(os.path.abspath(sys.argv[0])), shares_dump_dir)

def create_dump_dir():
     if os.path.isdir(shares_dump_dir):
        return
     if os.path.isfile(shares_dump_dir):
        print "%s should be a directory, not a file\n" % (shares_dump_dir,)
        sys.exit()
     print "%s directory doesn't exist, creating" % (shares_dump_dir,)
     os.mkdir(shares_dump_dir)

def zero_diff(filename):
   firstline = True
   for line in open(filename, "rt"):
       if firstline:
           firstline = False
           if line[0] != '*':
               return True
       else:
           return line[0] not in ('+','-','*')

def dump_share(share, diffto = ""):
    print "Dumping %(proto)s://%(host)s:%(port)s" \
        % {'proto': share['protocol'],
           'port': str(share['port']),
           'host': share['hostname']}
    cmd = os.path.join(scanners_path, "sqlscan")
    outfile = os.path.join(shares_dump_dir, share_save_str(share['protocol'], share['hostname'], share['port']))
    sp = subprocess.Popen("%(cmd)s -s %(proto)s -p %(port)s -dP %(dh)s %(du)s %(dd)s %(dfile)s %(host)s > %(sfile)s"
        % {'cmd': cmd,
           'proto': share['protocol'],
           'port': str(share['port']),
           'host': share['hostname'],
           'sfile': outfile,
           'dfile': "-u " + diffto if diffto != "" else "",
           'dh': "-dh " + db_host if len(db_host) > 0 else "",
           'du': "-du " + db_user if len(db_user) > 0 else "",
           'dd': "-dd " + db_database if len(db_database) > 0 else "",
           },
        shell = True, stdin=subprocess.PIPE)
    sp.communicate(input = db_password + "\n")
    sp.wait()
    return outfile

if __name__ == "__main__":
    if len(set(('?','-?','/?','-h','/h','help','--help')).intersection(sys.argv)) > 0:
        print sys.argv[0], '[diff [only]]'
        print "diff\tdump with diffs"
        print "only\tleave only dumps with non-empty diffs"
    try:
        db = connectdb()
    except:
        print "Unable to connect to the database, exiting."
        sys.exit()
    shares = db.cursor()
    shares.execute("""
        SELECT share_id, protocol, hostname, port FROM shares
        """)
    create_dump_dir()
    skipmode = 'only' in sys.argv
    if 'diff' in sys.argv:
        for share in shares.fetchall():
            savepath = share_save_path(share['protocol'], share['hostname'], share['port'])
            if os.path.isfile(savepath):
                dump = dump_share(share,savepath)
                if skipmode and zero_diff(dump):
                    os.unlink(dump)
            elif not skipmode:
                dump_share(share)
    else:
        for share in shares.fetchall():
            dump_share(share)

