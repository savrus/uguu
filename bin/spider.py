#!/usr/bin/python
#
# spider.py - high-level scanner
#
# Copyright 2010, savrus
# Read the COPYING file in the root of the source tree.
#

import psycopg2
import sys
import string
import re
import socket
import hashlib
import time
from common import connectdb, scanners_locale, run_scanner, filetypes, wait_until_next_scan, wait_until_next_scan_failed

def suffix(filename):
    dot = filename.rfind(".")
    if dot == -1:
        return ""
    else:
        return string.lower(filename[dot + 1:])


def tsprepare(string):
    relax = re.sub(r'(?u)\W', ' ', string, re.UNICODE)
    relax = re.sub(r'(?u)([Ss])(\d+)([Ee])(\d+)',
                   '\\1\\2\\3\\4 \\2 \\4', relax, re.UNICODE)
    return relax

fquery_select = "INSERT INTO files (share_id, sharepath_id, pathfile_id, sharedir_id, size, filename_id) VALUES "
fquery_values = "(%(s)s, %(p)s, %(f)s, %(did)s, %(sz)s, gfid(%(n)s, %(t)s, %(r)s))"
fquery_execute = "EXECUTE ifile (%(s)s, %(p)s, %(f)s, %(did)s, %(sz)s, %(n)s, %(t)s, %(r)s)"

class PsycoCache:
    def __init__(self, cursor):
        self.query = []
        self.fquery = []
        self.cursor = cursor
        self.cursor.execute("""
            PREPARE ifile(int, int, int, int, bigint, text, filetype, text) AS
            INSERT INTO files (share_id, sharepath_id, pathfile_id,
                sharedir_id, size, filename_id)
            VALUES ($1, $2, $3, $4, $5, gfid($6, $7, $8))
            """)
    def append(self, q, vars):
        self.query.append(self.cursor.mogrify(q, vars))
        if len(self.query) > 1024:
            self.commit()
    def commit(self):
        self.cursor.execute(string.join(self.query,";"))
        self.query = []
    def fappend(self, vars):
        #self.fquery.append(self.cursor.mogrify(fquery_values, vars))
        self.append(fquery_execute, vars)
        #if len(self.fquery) > 1024:
        #    self.fcommit()
    def fcommit(self):
        pass
        #self.query.append(fquery_select + string.join(self.fquery, ","))
        #self.fquery = []
        #self.commit()
    def allcommit(self):
        self.commit()

def scan_line(cursor, share, line, qcache):
    line = unicode(line, scanners_locale)
    if line[0] == "0":
        # 'path' type of line 
        try:
            l, id, path = string.split(s=line, maxsplit=2)
        except:
            l, id = string.split(s=line, maxsplit=2)
            path = ""
        id = int(id)
        qcache.append("INSERT INTO paths (share_id, sharepath_id, path, tspath) VALUES (%(s)s, %(id)s, %(p)s, to_tsvector(%(t)s))",
            {'s':share, 'id':id, 'p':path, 't':tsprepare(path)})
    else:
        # 'file' type of line
        try:
            l, path, file, size, dirid, items, name = string.split(s=line, maxsplit=6)
        except:
            l, path, file, size, dirid, items = string.split(s=line, maxsplit=6)
            name = ""
        path = int(path)
        file = int(file)
        size = int(size)
        dirid = int(dirid)
        items = int(items)
        if dirid > 0:
            # if directory then update paths table
            qcache.append("UPDATE paths SET parent_id = %(p)s, parentfile_id = %(f)s, items = %(i)s, size = %(sz)s WHERE share_id = %(s)s AND sharepath_id = %(d)s",
                {'p':path, 'f':file, 'i':items, 'sz':size, 's':share, 'd':dirid})
        if path == 0:
            # if share root then it's size is the share size
            qcache.append("UPDATE shares SET size = %(sz)s WHERE share_id = %(s)s", {'sz':size, 's':share})
        else:
            # not share root
            # save all info into the files table
            suf = suffix(name)
            type = filetypes.get(suf)
            qcache.fappend({'s':share, 'p':path, 'f':file, 'did':dirid,
                'sz':size, 'n':name, 't':type, 'r':tsprepare(name)})


def scan_share(db, share_id, proto, host, port, oldhash, command):
    cursor = db.cursor()
    hoststr = "%s://%s%s" % (proto, host, ":" + str(port) if port != 0 else "")
    address = socket.gethostbyname(host)
    print "[%s] Scanning %s (%s) ..." % (time.ctime(), hoststr, address)
    data = run_scanner(command, address, proto, port)
    hash = hashlib.sha256()
    scan_output = []
    for line in data.stdout:
        scan_output.append(line.strip('\n'))
        hash.update(line)
    if data.wait() != 0:
        # assume next_scan is far away from now and we do not have to
        # acquire lock on the shares table again
        shares.execute("""
            UPDATE shares SET next_scan = now() + %(w)s
            WHERE share_id = %(s)s;
            """, {'s':share_id, 'w': wait_until_next_scan_failed})
        db.commit()
        print "[%s] Scanning %s failed." % (time.ctime(), hoststr)
    elif hash.hexdigest() == oldhash:
        cursor.execute("""
            UPDATE shares SET last_scan = now()
            WHERE share_id = %(s)s
            """, {'s':share_id})
        db.commit()
        print "[%s] Scanning %s succeded. No changes found." \
              % (time.ctime(), hoststr)
    else:
        cursor.execute("DELETE FROM files WHERE share_id = %(id)s",
            {'id':share_id})
        cursor.execute("DELETE FROM paths WHERE share_id = %(id)s",
            {'id':share_id})
        qcache = PsycoCache(cursor)
        for line in scan_output:
            scan_line(cursor, share_id, line, qcache)
        qcache.allcommit()
        cursor.execute("""
            UPDATE shares
            SET last_scan = now(), hash = %(h)s
            WHERE share_id = %(s)s
            """, {'s':share_id,
                  'h': hash.hexdigest()
                  })
        db.commit()
        print "[%s] Scanning %s succeded. Database updated." \
              % (time.ctime(), hoststr)


if __name__ == "__main__":
    try:
        db = connectdb()
    except:
        print "Unable to connect to the database, exiting."
        sys.exit()
    shares = db.cursor()
    proceed = True
    while proceed:
        shares.execute("""
            LOCK TABLE shares IN SHARE ROW EXCLUSIVE MODE;
            
            SELECT share_id, shares.protocol, hostname, port, hash, scan_command
            FROM shares
            LEFT JOIN scantypes ON shares.scantype_id = scantypes.scantype_id
            WHERE state = 'online' AND (next_scan IS NULL OR next_scan < now())
            ORDER BY next_scan LIMIT 1
            """)
        if shares.rowcount == 0:
            proceed = False
            db.rollback()
            break
        id, proto, host, port, hash, command = shares.fetchone()
        shares.execute("""
            UPDATE shares SET next_scan = now() + %(w)s
            WHERE share_id = %(s)s;
            """, {'s':id, 'w': wait_until_next_scan})
        # release lock on commit
        db.commit()
        scan_share(db, id, proto, host, port, hash, command)

