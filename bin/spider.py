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


def scan_line(cursor, share, line):
    line = unicode(line, scanners_locale)
    if line[0] == "0":
        # 'path' type of line 
        try:
            l, id, path = string.split(s=line, maxsplit=2)
        except:
            l, id = string.split(s=line, maxsplit=2)
            path = ""
        id = int(id)
        cursor.execute("""
            INSERT INTO paths (share_id, sharepath_id, path, tspath)
            VALUES (%(s)s, %(id)s, %(p)s, to_tsvector(%(t)s))
            """, {'s':share, 'id':id, 'p':path, 't':tsprepare(path)})
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
            cursor.execute("""
                UPDATE paths SET parent_id = %(p)s,
                                 parentfile_id = %(f)s,
                                 items = %(i)s,
                                 size = %(sz)s
                WHERE share_id = %(s)s AND sharepath_id = %(d)s
                """, {'p':path, 'f':file, 'i':items,
                      'sz':size, 's':share, 'd':dirid})
        if path == 0:
            # if share root then it's size is the share size
            cursor.execute("""
                UPDATE shares SET size = %(sz)s
                WHERE share_id = %(s)s
                """, {'sz':size, 's':share})
        else:
            # not share root
            # save all info into the files table
            cursor.execute("""
                SELECT filename_id FROM filenames WHERE name = %(n)s
                """, {'n':name})
            if cursor.rowcount > 0:
                filename, = cursor.fetchone()
            else:
                suf = suffix(name)
                type = filetypes.get(suf)
                cursor.execute("""
                    INSERT INTO filenames (name, type, tsname)
                    VALUES (%(n)s, %(t)s, to_tsvector('uguu', %(r)s))
                    """, {'n':name, 't':type, 'r':tsprepare(name)})
                cursor.execute("SELECT * FROM lastval()")
                filename, = cursor.fetchone()
            cursor.execute("""
                INSERT INTO files (share_id,
                                   sharepath_id,
                                   pathfile_id,
                                   sharedir_id,
                                   size,
                                   filename_id)
                VALUES (%(s)s, %(p)s, %(f)s, %(did)s, %(sz)s, %(fn)s)
                """, {'s':share, 'p':path, 'f':file, 'did':dirid, 'sz':size,
                      'fn':filename })


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
        db.rollback()
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
            UPDATE shares SET last_scan = now() + %(w)s
            WHERE share_id = %(s)s
            """, {'s':share_id, 'w': wait_until_next_scan})
        db.commit()
        print "[%s] Scanning %s succeded. No changes found." \
              % (time.ctime(), hoststr)
    else:
        cursor.execute("DELETE FROM files WHERE share_id = %(id)s",
            {'id':share_id})
        cursor.execute("DELETE FROM paths WHERE share_id = %(id)s",
            {'id':share_id})
        for line in scan_output:
            scan_line(cursor, share_id, line)
        cursor.execute("""
            UPDATE shares
            SET last_scan = now() + %(w)s, hash = %(h)s
            WHERE share_id = %(s)s
            """, {'s':share_id,
                  'w': wait_until_next_scan,
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

