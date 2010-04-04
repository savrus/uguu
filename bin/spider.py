#!/usr/bin/env python
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
import tempfile
import os
import sys
import traceback
import datetime
import psycopg2.extensions
from common import connectdb, log, scanners_locale, run_scanner, filetypes, wait_until_next_scan, wait_until_next_scan_failed, max_lines_from_scanner, sharestr


# python 2.5 compitible shitcode
def kill_process(process):
    if sys.version_info[:2] < (2, 6):
        if os.name == 'nt':
            subprocess.Popen("taskkill /F /T /PID %s >nul 2>nul"% process.pid, shell=True)
        else:
            os.kill(process.pid, -9)
    else:
        process.kill()


filetypes_reverse = dict([(x,y) for y in filetypes.keys()
                                for x in filetypes[y]])

def suffix(filename):
    dot = filename.rfind(".")
    if dot == -1:
        return ""
    else:
        return string.lower(filename[dot + 1:])


def tsprepare(string):
    relax = re.sub(r'(?u)\W', ' ', string, re.UNICODE)
    relax = re.sub(r'(?u)([Ss])(\d+)([Ee])(\d+)',
                   '\\1\\2\\3\\4 \\1\\2 \\3\\4 \\2 \\4 ', relax, re.UNICODE)
    return relax

fquery_select = "INSERT INTO files (share_id, sharepath_id, pathfile_id, sharedir_id, size, name, type, tsname, tspath) VALUES "
fquery_values = "(%(s)s, %(p)s, %(f)s, %(did)s, %(sz)s, %(n)s, %(t)s, to_tsvector('uguu', %(r)s), to_tsvector('uguu', %(rt)s))"

class PsycoCache:
    def __init__(self, cursor):
        self.query = []
        self.fquery = []
        self.cursor = cursor
        self.totalsize = 0
    def append(self, q, vars):
        self.query.append(self.cursor.mogrify(q, vars))
        if len(self.query) > 1024:
            self.commit()
    def commit(self):
        if len(self.query) > 0:
            self.cursor.execute(string.join(self.query,";"))
            self.query = []
    def fappend(self, vars):
        self.fquery.append(self.cursor.mogrify(fquery_values, vars))
        if len(self.fquery) > 1024:
            self.fcommit()
    def fcommit(self):
        if len(self.fquery) > 0:
            self.query.append(fquery_select + string.join(self.fquery, ","))
            self.fquery = []
        self.commit()
    def allcommit(self):
        self.fcommit()

def scan_line(cursor, share, line, qcache, paths_buffer):
    try:
        line = unicode(line, scanners_locale)
    except:
        log("Non utf-8 line occured: '%s'.", line)
        line = string.join([(lambda x: x if x in string.printable else "\\%#x" % ord(c))(c) for c in line], "")        
    if line[0] == "0":
        # 'path' type of line 
        try:
            l, id, path = string.split(s=line, sep=' ', maxsplit=2)
        except:
            l, id = string.split(s=line, sep=' ', maxsplit=2)
            path = ""
        id = int(id)
        paths_buffer[id] = tsprepare(path)
        qcache.append("INSERT INTO paths (share_id, sharepath_id, path) VALUES (%(s)s, %(id)s, %(p)s)",
            {'s':share, 'id':id, 'p':path})

    else:
        # 'file' type of line
        try:
            l, path, file, size, dirid, items, name = string.split(s=line, sep=' ', maxsplit=6)
        except:
            l, path, file, size, dirid, items = string.split(s=line, sep=' ', maxsplit=6)
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
            paths_buffer.pop(dirid)
        if path == 0:
            # if share root then it's size is the share size
            qcache.totalsize = size
        else:
            # not share root
            # save all info into the files table
            suf = suffix(name)
            type = filetypes_reverse.get(suf) if dirid == 0 else 'dir'
            qcache.fappend({'s':share, 'p':path, 'f':file, 'did':dirid,
                'sz':size, 'n':name, 't':type, 'r':tsprepare(name), 'rt':paths_buffer[path]})


def scan_share(db, share_id, proto, host, port, oldhash, command):
    cursor = db.cursor()
    hoststr = sharestr(proto, host, port)
    address = socket.gethostbyname(host)
    log("Scanning %s (%s) ...", (hoststr, address))
    start = datetime.datetime.now()
    data = run_scanner(command, address, proto, port)
    save = tempfile.TemporaryFile(bufsize=-1)
    hash = hashlib.sha256()
    line_count = 0
    for line in data.stdout:
        line_count += 1
        if line_count > max_lines_from_scanner:
            kill_process(data)
            data.stdout.close()
            data.wait()
            log("Scanning %s failed. Too many lines from scanner (elapsed time %s).", (hoststr, datetime.datetime.now() - start))
            return
        hash.update(line)
        save.write(line)
    if data.wait() != 0:
        # assume next_scan is far away from now and we do not have to
        # acquire lock on the shares table again
        shares.execute("""
            UPDATE shares SET next_scan = now() + %(w)s
            WHERE share_id = %(s)s;
            """, {'s':share_id, 'w': wait_until_next_scan_failed})
        db.commit()
        log("Scanning %s failed (elapsed time %s).", (hoststr, datetime.datetime.now() - start))
    elif hash.hexdigest() == oldhash:
        cursor.execute("""
            UPDATE shares SET last_scan = now()
            WHERE share_id = %(s)s
            """, {'s':share_id})
        db.commit()
        log("Scanning %s succeded. No changes found (scan time %s).", (hoststr, datetime.datetime.now() - start))
    else:
        scan_time = datetime.datetime.now() - start
        start = datetime.datetime.now()
        cursor.execute("""
            DELETE FROM files WHERE share_id = %(s)s;
            DELETE FROM paths WHERE share_id = %(s)s;
            """, {'s':share_id})
        qcache = PsycoCache(cursor)
        paths_buffer = dict()
        save.seek(0)
        for line in save:
            scan_line(cursor, share_id, line.strip('\n'), qcache, paths_buffer)
        qcache.allcommit()
        save.close()
        cursor.execute("""
            UPDATE shares
            SET size = %(sz)s, last_scan = now(), hash = %(h)s
            WHERE share_id = %(s)s
            """, {'s':share_id, 'h': hash.hexdigest(), 'sz': qcache.totalsize})
        db.commit()
        log("Scanning %s succeded. Database updated (scan time %s, update time %s).", (hoststr, scan_time, datetime.datetime.now() - start))


if __name__ == "__main__":
    try:
        db = connectdb()
    except:
        print "Unable to connect to the database, exiting."
        sys.exit()
    shares = db.cursor()
    while True:
        db.set_isolation_level(psycopg2.extensions.ISOLATION_LEVEL_AUTOCOMMIT)
        shares.execute("""
            SELECT share_id, shares.protocol, hostname, port, hash, scan_command
            FROM shares
            LEFT JOIN scantypes ON shares.scantype_id = scantypes.scantype_id
            WHERE state = 'online' AND (next_scan IS NULL OR next_scan < now())
            ORDER BY next_scan LIMIT 1
            """)
        if shares.rowcount == 0:
            break
        id, proto, host, port, hash, command = shares.fetchone()
        shares.execute("""
            UPDATE shares SET next_scan = now() + %(w)s
            WHERE share_id = %(s)s AND (next_scan IS NULL OR next_scan < now())
            """, {'s':id, 'w': wait_until_next_scan})
        if shares.statusmessage != 'UPDATE 1':
            continue
        db.set_isolation_level(psycopg2.extensions.ISOLATION_LEVEL_READ_COMMITTED)
        try:
            scan_share(db, id, proto, host, port, hash, command)
        except:
            log("Scanning %s failed with a crash. Something unexpected happened. Exception trace:", sharestr(proto, host, port))
            traceback.print_exc()
            db.rollback()

