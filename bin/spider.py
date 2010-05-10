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
import tempfile
import hashlib
import os
import sys
import traceback
import shutil
import datetime
import psycopg2.extensions
from common import connectdb, log, scanners_locale, run_scanner, filetypes, wait_until_next_scan, wait_until_next_scan_failed, max_lines_from_scanner, sharestr, share_save_path, share_save_str, quote_for_shell, shares_save_dir

# if patch is longer than whole contents / patch_fallback, then fallback
# to non-patching mode
patch_fallback = 1

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

fquery_append = "INSERT INTO files (tree_id, treepath_id, pathfile_id, treedir_id, size, name, type, tsname, tspath) VALUES "
fquery_values = "(%(i)s, %(p)s, %(f)s, %(did)s, %(sz)s, %(n)s, %(t)s, to_tsvector('uguu', %(r)s), to_tsvector('uguu', %(rt)s))"

class PsycoCache:
    def __init__(self, cursor):
        self.query = []
        self.fquery = []
        self.cursor = cursor
        self.totalsize = -1
        self.stat_padd = 0
        self.stat_pdelete = 0
        self.stat_pmodify = 0
        self.stat_fadd = 0
        self.stat_fdelete = 0
        self.stat_fmodify = 0
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
            self.query.append(fquery_append + string.join(self.fquery, ","))
            self.fquery = []
        self.commit()
    def allcommit(self):
        self.fcommit()

class PathInfo:
    def __init__(self):
        self.tspath = ""
        self.delta = 0
        self.modify = False

def unicodize_line(line):
    try:
        line = unicode(line, scanners_locale)
    except:
        log("Non utf-8 line occured: '%s'.", line)
        line = string.join([(lambda x: x if x in string.printable else "\\%#x" % ord(c))(c) for c in line], "")        
    return line

def scan_line_patch(cursor, tree, line, qcache, paths_buffer):
    line = unicodize_line(line)
    if line[2] == "0":
        # 'path' type of line 
        try:
            act, l, id, path = string.split(s=line, sep=' ', maxsplit=3)
        except:
            act, l, id = string.split(s=line, sep=' ', maxsplit=3)
            path = ""
        id = int(id)
        if act == '+':
            qcache.stat_padd += 1
            paths_buffer[id] = PathInfo()
            paths_buffer[id].tspath = tsprepare(path)
            qcache.append("INSERT INTO paths (tree_id, treepath_id, path) VALUES (%(t)s, %(id)s, %(p)s)",
                {'t':tree, 'id':id, 'p':path})
        elif act == '-':
            qcache.stat_pdelete += 1
            qcache.append("DELETE FROM paths WHERE tree_id = %(t)s AND treepath_id = %(id)s",
                {'t':tree, 'id':id})
        elif act == '*':
            qcache.stat_pmodify += 1
            paths_buffer[id] = PathInfo()
            paths_buffer[id].modify = True
            paths_buffer[id].tspath = tsprepare(path)
    else:
        # 'file' type of line
        try:
            act, l, path, file, size, dirid, items, name = string.split(s=line, sep=' ', maxsplit=7)
        except:
            act, l, path, file, size, dirid, items = string.split(s=line, sep=' ', maxsplit=7)
            name = ""
        path = int(path)
        file = int(file)
        size = int(size)
        dirid = int(dirid)
        items = int(items)
        if (act == '+' or act == '*') and dirid > 0:
            # if directory then update paths table
            qcache.append("""
                UPDATE paths SET parent_id = %(p)s, parentfile_id = %(f)s, items = %(i)s, size = %(sz)s
                WHERE tree_id = %(t)s AND treepath_id = %(d)s
                """, {'p':path, 'f':file, 'i':items, 'sz':size, 't':tree, 'd':dirid})
            if act == '+':
                paths_buffer.pop(dirid)
        if path == 0:
            # if share root then it's size is the share size
            qcache.totalsize = size
        else:
            # not share root
            suf = suffix(name)
            type = filetypes_reverse.get(suf) if dirid == 0 else 'dir'
            if act == '+':
                qcache.stat_fadd += 1
                if paths_buffer[path].modify:
                    qcache.append("""
                        UPDATE files SET pathfile_id = pathfile_id + 1
                        WHERE tree_id = %(i)s AND treepath_id = %(p)s
                            AND pathfile_id >= %(f)s
                        """, {'i':tree, 'p':path, 'f':file})
                    qcache.append(fquery_append + fquery_values,
                        {'i':tree, 'p':path, 'f':file, 'did':dirid, 'sz':size,
                         'n':name, 't':type, 'r':tsprepare(name), 'rt':paths_buffer[path].tspath})
                    paths_buffer[path].delta += 1
                else:
                    qcache.fappend({'i':tree, 'p':path, 'f':file, 'did':dirid, 'sz':size,
                         'n':name, 't':type, 'r':tsprepare(name), 'rt':paths_buffer[path].tspath})
            elif act == '-':
                qcache.stat_fdelete += 1
                qcache.append("""
                    DELETE FROM files
                    WHERE tree_id = %(t)s AND treepath_id = %(p)s AND pathfile_id = %(f)s;
                    UPDATE files SET pathfile_id = pathfile_id - 1
                    WHERE tree_id = %(t)s AND treepath_id = %(p)s
                        AND pathfile_id > %(f)s
                    """, {'t': tree, 'p': path, 'f': file + paths_buffer[path].delta})
                paths_buffer[path].delta -= 1
            elif act == '*':
                qcache.stat_fmodify += 1
                qcache.append("""
                    UPDATE files SET size = %(sz)s
                    WHERE tree_id = %(t)s AND treepath_id = %(p)s AND pathfile_id = %(f)s
                    """,  {'t': tree, 'p': path, 'f': file, 'sz': size})

def scan_share(db, share_id, proto, host, port, tree_id, oldhash, command):
    cursor = db.cursor()
    hoststr = sharestr(proto, host, port)
    savepath = share_save_path(proto, host, port)
    patchmode = tree_id != None and os.path.isfile(savepath)
    address = socket.gethostbyname(host)
    log("Scanning %s (%s) ...", (hoststr, address))
    start = datetime.datetime.now()
    if patchmode:
        data = run_scanner(command, address, proto, port, "-u " + quote_for_shell(savepath))
    else:
        data = run_scanner(command, address, proto, port)
    save = tempfile.TemporaryFile(bufsize=-1)
    line_count = 0
    line_count_patch = 0
    hash = hashlib.md5()
    for line in data.stdout:
        line_count += 1
        if line[0] in ('+', '-', '*'):
            line_count_patch += 1
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
        cursor.execute("""
            UPDATE shares SET next_scan = now() + %(w)s
            WHERE share_id = %(s)s;
            """, {'s':share_id, 'w': wait_until_next_scan_failed})
        log("Scanning %s failed (elapsed time %s).", (hoststr, datetime.datetime.now() - start))
        return
    if patchmode and (line_count_patch > (line_count - line_count_patch) / patch_fallback):
        log("Patch is too long for %s (patch %s, non-patch %s). Fallback to non-patching mode", (hoststr, line_count_patch, line_count - line_count_patch))
        patchmode = False
    db.set_isolation_level(psycopg2.extensions.ISOLATION_LEVEL_READ_COMMITTED)
    scan_time = datetime.datetime.now() - start
    start = datetime.datetime.now()
    if tree_id is None:
        cursor.execute("""
            INSERT INTO trees (share_id, hash) VALUES (%(s)s, %(h)s)
            RETURNING tree_id
            """, {'s': share_id, 'h': hash.hexdigest()})
        tree_id = cursor.fetchone()['tree_id']
    qcache = PsycoCache(cursor)
    paths_buffer = dict()
    save.seek(0)
    if patchmode and (oldhash == None or save.readline() != "* " + oldhash + "\n"):
        save.seek(0)
        patchmode = False
        log("Awaited MD5 digest from scanner doesn't match the one from the database. Fallback to non-patching mode.")
    if patchmode:
        for line in save:
            if line[0] not in ('+', '-', '*'):
                break
            scan_line_patch(cursor, tree_id, line.strip('\n'), qcache, paths_buffer)
    else:
        cursor.execute("DELETE FROM paths WHERE tree_id = %(t)s", {'t':tree_id})
        for line in save:
            if line[0] in ('+', '-', '*'):
                continue
            scan_line_patch(cursor, tree_id, "+ " + line.strip('\n'), qcache, paths_buffer)
    qcache.allcommit()
    try:
        save.seek(0)
        file = open(savepath, 'wb')
        shutil.copyfileobj(save, file)
        file.close()
    except:
        log("Failed to save contents of %s to file %s.", (hoststr, savepath))
    save.close()
    cursor.execute("""
        UPDATE shares SET tree_id = %(t)s, last_scan = now() WHERE share_id = %(s)s;
        UPDATE trees SET hash = %(h)s WHERE tree_id = %(t)s;
        """, {'s': share_id, 't': tree_id, 'h': hash.hexdigest()})
    if qcache.totalsize >= 0:
        cursor.execute("""
            UPDATE shares SET size = %(sz)s WHERE share_id = %(s)s;
            """, {'s':share_id, 'sz': qcache.totalsize})
    db.commit()
    if patchmode:
        deleted = qcache.stat_pdelete + qcache.stat_fdelete
        added = qcache.stat_padd + qcache.stat_fadd
        modified = qcache.stat_fmodify
        log("Scanning %s succeded. Database updated in patching mode: delete %s, add %s, modify %s (scan time %s, update time %s).",
            (hoststr, str(deleted), str(added), str(modified), scan_time, datetime.datetime.now() - start))
    else:
        log("Scanning %s succeded. Database updated in non-patching mode (scan time %s, update time %s).",
            (hoststr, scan_time, datetime.datetime.now() - start))

def create_save():
     if os.path.isdir(shares_save_dir):
        return
     if os.path.isfile(shares_save_dir):
        raise NameError("%s should be a directory, not a file\n" % (shares_save_dir,))
     log("%s directory doesn't exist, creating" % (shares_save_dir,))
     os.mkdir(shares_save_dir)

if __name__ == "__main__":
    try:
        db = connectdb()
    except:
        print "Unable to connect to the database, exiting."
        sys.exit()
    create_save()
    shares = db.cursor()
    while True:
        db.set_isolation_level(psycopg2.extensions.ISOLATION_LEVEL_AUTOCOMMIT)
        shares.execute("""
            SELECT share_id, tree_id, hash, shares.protocol, hostname, port, scan_command
            FROM shares
            LEFT JOIN scantypes USING (scantype_id)
            LEFT JOIN trees USING (share_id, tree_id)
            WHERE state = 'online' AND (next_scan IS NULL OR next_scan < now())
            ORDER BY next_scan LIMIT 1
            """)
        if shares.rowcount == 0:
            break
        id, tree_id, hash, proto, host, port, command = shares.fetchone()
        shares.execute("""
            UPDATE shares SET next_scan = now() + %(w)s
            WHERE share_id = %(s)s AND (next_scan IS NULL OR next_scan < now())
            """, {'s':id, 'w': wait_until_next_scan})
        if shares.statusmessage != 'UPDATE 1':
            continue
        try:
            scan_share(db, id, proto, host, port, tree_id, hash, command)
        except:
            log("Scanning %s failed with a crash. Something unexpected happened. Exception trace:", sharestr(proto, host, port))
            traceback.print_exc()
            db.rollback()

