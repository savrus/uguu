#!/usr/bin/env python
#
# spider.py - high-level scanner
#
# Copyright 2010, savrus
# Copyright (c) 2010, Radist <radist.nt@gmail.com>
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
import datetime
import time
import shutil
import subprocess
import psycopg2.extensions
from common import connectdb, log, scanners_locale, run_scanner, scanners_logging, filetypes, wait_until_next_scan, wait_until_next_scan_failed, max_lines_from_scanner, sharestr, share_save_path, quote_for_shell, shares_save_dir

# max low-level scanner subprocesses count
# to start the required number of scanners (N), spider will query N*select_multiplier shares,
# so set select_multiplier between 1 and max spider process count to minimize excessive queries and non-required selected data  
max_scanners = 2
select_multiplier = 1.0

# interval between low-level scanner subprocesses polling (in seconds)  
wait_scanners_sleep = 10

# if patch is longer than whole contents / patch_fallback, then fallback
# to non-patching mode
patch_fallback = 0.8

# python 2.5 compitible shitcode
def kill_process(process):
    if sys.version_info[:2] < (2, 6):
        if os.name == 'nt':
            subprocess.Popen("taskkill /F /T /PID %s >nul 2>nul"% process.pid, shell=True)
        else:
            os.kill(process.pid, 9)
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

fquery_append = "INSERT INTO %sfiles (tree_id, treepath_id, pathfile_id, treedir_id, size, name, type, tsname, tspath) VALUES "
fquery_values = "(%(i)s, %(p)s, %(f)s, %(did)s, %(sz)s, %(n)s, %(t)s, to_tsvector('uguu', %(r)s), to_tsvector('uguu', %(rt)s))"

class PsycoCache:
    def __init__(self, self.cursor):
        self.query = []
        self.fquery = []
        self.self.cursor = self.cursor
        self.totalsize = -1
        self.stat_padd = 0
        self.stat_pdelete = 0
        self.stat_pmodify = 0
        self.stat_fadd = 0
        self.stat_fdelete = 0
        self.stat_fmodify = 0
    def append(self, q, vars):
        self.query.append(self.self.cursor.mogrify(q, vars))
        if len(self.query) > 1024:
            self.commit()
    def commit(self):
        if len(self.query) > 0:
            self.self.cursor.execute(string.join(self.query,";"))
            self.query = []
    def fappend(self, vars):
        self.fquery.append(self.self.cursor.mogrify(fquery_values, vars))
        if len(self.fquery) > 1024:
            self.fcommit()
    def fcommit(self):
        if len(self.fquery) > 0:
            self.query.append((fquery_append % '') + string.join(self.fquery, ","))
            self.fquery = []
        self.commit()
    def allcommit(self):
        self.fcommit()

class PathInfo:
    def __init__(self):
        self.tspath = ""
        self.modify = False

no_path = PathInfo()        

def unicodize_line(line):
    try:
        line = unicode(line, scanners_locale)
    except:
        log("Non utf-8 line occured: '%s'.", line)
        line = string.join([(lambda x: x if x in string.printable else "\\%#x" % ord(c))(c) for c in line], "")        
    return line

def scan_line_patch(self.cursor, tree, line, qcache, paths_buffer):
    line = unicodize_line(line)
    if line[2] == "0":
        # 'path' type of line 
        try:
            act, l, id, path = string.split(s=line, sep=' ', maxsplit=3)
        except:
            act, l, id = string.split(s=line, sep=' ', maxsplit=3)
            path = ""
        del l
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
            if paths_buffer.pop(dirid, no_path).modify:
                qcache.append("SELECT push_path_files(%(t)s, %(d)s)", {'t': tree, 'd': dirid})
        if path == 0:
            # if share root then it's size is the share size
            qcache.totalsize = size
        else:
            # not share root
            if act == '+':
                suf = suffix(name)
                type = filetypes_reverse.get(suf) if dirid == 0 else 'dir'
                qcache.stat_fadd += 1
                if paths_buffer[path].modify:
                    qcache.append((fquery_append % 'new') + fquery_values,
                        {'i':tree, 'p':path, 'f':file, 'did':dirid, 'sz':size,
                         'n':name, 't':type, 'r':tsprepare(name), 'rt':paths_buffer[path].tspath})
                else:
                    qcache.fappend({'i':tree, 'p':path, 'f':file, 'did':dirid, 'sz':size,
                         'n':name, 't':type, 'r':tsprepare(name), 'rt':paths_buffer[path].tspath})
            elif act == '-':
                qcache.stat_fdelete += 1
                qcache.append("""
                    DELETE FROM files
                    WHERE tree_id = %(t)s AND treepath_id = %(p)s AND pathfile_id = %(f)s;
                    """, {'t': tree, 'p': path, 'f': file})
            elif act == '*':
                qcache.stat_fmodify += 1
                qcache.append("""
                    UPDATE files SET size = %(sz)s
                    WHERE tree_id = %(t)s AND treepath_id = %(p)s AND pathfile_id = %(f)s
                    """,  {'t': tree, 'p': path, 'f': file, 'sz': size})

class scan_share:
    def __init__(self, db, share_id, tree_id, command):
        self.db = db
        self.share_id = share_id 
        self.tree_id = tree_id
        self.cursor = self.db.cursor()
        self.oldhash = None
        self.hoststr = None
        self.savepath = None
        self.patchmode = None
        self.process = None
        self.start = None
        self.scan_time = None
    def unlock(self):
        # release advisory lock
        # Attention: be careful, don't skip this!!!
        self.cursor.execute("SELECT pg_advisory_unlock(%s)", (self.tree_id,)) 
    def run(self, proto, host, port):
        self.hoststr = sharestr(proto, host, port)
        self.savepath = share_save_path(proto, host, port)
        try:
            # asquire advisory lock for tree_id, column locking couldn't be used because another share's update will release locking
            self.cursor.execute("SELECT pg_try_advisory_lock(tree_id) as locked, hash FROM ONLY trees WHERE tree_id=%(t)s", {'t': tree_id})
            row = self.cursor.fetchone()
            if not row['locked']:
                raise UserWarning
            self.oldhash = row['hash']
        except:
            # if other spider instance didn't complete scanning, do nothing
            # side effect is backing-off the next scan
            log("Scanning %s is running too long in another spider instance or database error.", (self.hoststr,))
            self.db.rollback()
            raise UserWarning
        self.patchmode = self.oldhash != None and os.path.isfile(self.savepath)
        try:
            address = socket.gethostbyname(host)
        except:
            try:
                log("Name resolution failed for %s.", (self.hoststr,))
                self.db.rollback()
            finally:
                self.unlock()
                raise UserWarning
        log("Starting scanner for %s (%s) ...", (self.hoststr, address))
        self.start = datetime.datetime.now()
        if self.patchmode:
            self.process = run_scanner(command, address, proto, port, "-u " + quote_for_shell(self.savepath))
        else:
            self.process = run_scanner(command, address, proto, port)
    def check_finished(self):
        if self.process.poll() is not None:
            if self.scan_time is None:
                self.scan_time = datetime.datetime.now() - self.start
                if scanners_logging:
                    log('Finished low-level scanner for %s, see log below', (self.hoststr,))
                    shutil.copyfileobj(self.process.stderr, sys.stderr)
                self.process.stderr.close()
                if self.process.returncode != 0:
                    try:
                        self.cursor.execute("""
                            UPDATE shares SET next_scan = now() + %(w)s
                            WHERE share_id = %(s)s;
                            """, {'s': self.share_id, 'w': wait_until_next_scan_failed})
                        self.db.commit()
                        log("Scanning %s failed with return code %s (elapsed time %s).", (self.hoststr, self.process.returncode, self.scan_time))
                        raise UserWarning
                    finally:
                        self.unlock()
                line_count = 0
                line_count_patch = 0
                hash = hashlib.md5()
                self.process.stdout.seek(0)
                for line in self.process.stdout:
                    line_count += 1
                    if line_count > max_lines_from_scanner:
                        try:
                            self.process.stdou.close()
                            log("Scanning %s failed. Too many lines from scanner (elapsed time %s).", (self.hoststr, self.scan_time))
                            self.db.rollback()
                            raise UserWarning
                        finally:
                            self.unlock()                            
                    if line[0] in ('+', '-', '*'):
                        line_count_patch += 1
                    hash.update(line)
                self.process.stdout.seek(0)
                self.scan_time = datetime.datetime.now() - self.start
                if self.patchmode and (line_count_patch > (line_count - line_count_patch) / patch_fallback):
                    log("Patch is too long for %s (patch %s, non-patch %s). Fallback to non-patching mode", (self.hoststr, line_count_patch, line_count - line_count_patch))
                    self.patchmode = False
                scanhash = self.process.stdout.readline()
                if self.patchmode and (scanhash != "* " + self.oldhash + "\n"):
                    self.process.stdout.seek(0)
                    self.patchmode = False
                    log("MD5 digest from scanner (%s) doesn't match the one from the database (%s) for %s. Fallback to non-patching mode.", (string.strip(scanhash[1:]), self.oldhash, self.hoststr))
                self.oldhash = hash.hexdigest()
            return True
    def update_db(self):
        try:
            self.start = datetime.datetime.now()
            qcache = PsycoCache(self.cursor)
            paths_buffer = dict()
            if self.patchmode:
                self.cursor.execute("""
                    CREATE TEMPORARY TABLE newfiles (
                        LIKE files INCLUDING DEFAULTS
                        ) ON COMMIT DROP;
                    CREATE INDEX newfiles_path ON newfiles(treepath_id);
                    """)
                for line in self.process.stdout:
                    if line[0] not in ('+', '-', '*'):
                        break
                    scan_line_patch(self.cursor, self.tree_id, line.strip('\n'), qcache, paths_buffer)
                for (dirid, pinfo) in paths_buffer.iteritems():
                    if pinfo.modify:
                        qcache.append("SELECT push_path_files(%(t)s, %(d)s)", {'t': self.tree_id, 'd': dirid})
            else:
                self.cursor.execute("DELETE FROM paths WHERE tree_id = %(t)s", {'t': self.tree_id})
                for line in self.process.stdout:
                    if line[0] in ('+', '-', '*'):
                        continue
                    scan_line_patch(self.cursor, self.tree_id, "+ " + line.strip('\n'), qcache, paths_buffer)
            qcache.allcommit()
            try:
                if os.path.isfile(self.savepath):
                    shutil.move(self.savepath, self.savepath + ".old")
                self.process.stdout.seek(0)
                file = open(self.savepath, 'wb')
                shutil.copyfileobj(self.process.stdout, file)
                file.close()
            except:
                log("Failed to save contents of %s to file %s.", (self.hoststr, self.savepath))
                traceback.print_exc()
            self.process.stdout.close()
            self.cursor.execute("""
                UPDATE shares SET last_scan = now(), next_scan = now() + %(w)s WHERE share_id = %(s)s;
                UPDATE trees SET hash = %(h)s WHERE tree_id = %(t)s;
                """, {'s': self.share_id, 't': self.tree_id, 'h': self.oldhash, 'w': wait_until_next_scan})
            if qcache.totalsize >= 0:
                self.cursor.execute("""
                    UPDATE shares SET size = %(sz)s WHERE share_id = %(s)s;
                    """, {'s': self.share_id, 'sz': qcache.totalsize})
            self.db.commit()
            if self.patchmode:
                deleted = qcache.stat_pdelete + qcache.stat_fdelete
                added = qcache.stat_padd + qcache.stat_fadd
                modified = qcache.stat_fmodify
                log("Scanning %s succeded. Database updated in patching mode: delete %s, add %s, modify %s (scan time %s, update time %s).",
                    (self.hoststr, str(deleted), str(added), str(modified), self.scan_time, datetime.datetime.now() - self.start))
            else:
                log("Scanning %s succeded. Database updated in non-patching mode (scan time %s, update time %s).",
                    (self.hoststr, self.scan_time, datetime.datetime.now() - self.start))
        finally:
            self.unlock()
    def handle_exception(self, exc):
        if isinstance(exc, KeyboardInterrupt):
            log("Interrupted by user. Exiting")
            self.db.rollback()
            sys.exit(0)
        elif isinstance(exc, psycopg2.IntegrityError):
            try:
                now = int(time.time())
                log("SQL Integrity violation while scanning %s. Rename old contents with suffix %s. Next scan to be in non-patching mode", (self.hoststr, now))
                traceback.print_exc()
                self.db.rollback()
            finally:
                self.unlock()
            if os.path.isfile(self.savepath):
                shutil.move(self.savepath, self.savepath + "." + str(now))
            self.savepath += ".old"
            if os.path.isfile(self.savepath):
                shutil.move(self.savepath, self.savepath + "." + str(now))
        elif isinstance(exc, UserWarning):
            pass
        elif isinstance(exc, Exception):
            try:
                log("Scanning %s failed with a crash. Something unexpected happened. Exception trace:", (self.hoststr,))
                traceback.print_exc()
                self.db.rollback()
            finally:
                self.unlock()
        else:
            raise

def create_save_dir():
    if os.path.isdir(shares_save_dir):
        return
    if os.path.isfile(shares_save_dir):
        raise NameError, "%s should be a directory, not a file\n" % shares_save_dir
    log("%s directory doesn't exist, creating" % shares_save_dir)
    os.mkdir(shares_save_dir)
    


if __name__ == "__main__":
    try:
        db_scan = connectdb()
        db_select = connectdb()
    except:
        print "Unable to connect to the database, exiting."
        sys.exit()
    create_save_dir()
    db_scan.set_isolation_level(psycopg2.extensions.ISOLATION_LEVEL_READ_COMMITTED)
    db_select.set_isolation_level(psycopg2.extensions.ISOLATION_LEVEL_AUTOCOMMIT)
    shares = db_select.cursor()
    locker = db_select.cursor()
    scanners = list()
    while True:
        while len(scanners) < max_scanners:
            shares.execute("""
                SELECT share_id, tree_id, shares.protocol, hostname, port, scan_command
                FROM shares
                LEFT JOIN scantypes USING (scantype_id)
                WHERE state = 'online' AND (next_scan IS NULL OR next_scan < now()) %s
                ORDER BY next_scan NULLS FIRST LIMIT %%(limit)s
                """ % ('AND share_id not in %(queued)s' if len(scanners) > 0 else ''), {
                    'limit': int(select_multiplier * (max_scanners - len(scanners))),
                    'queued': tuple(s.share_id for s in scanners)
                    })
            if shares.rowcount == 0:
                break
            for id, tree_id, proto, host, port, command in shares:
                locker.execute("""
                    UPDATE shares SET next_scan = now() + %(w)s
                    WHERE share_id = %(s)s AND (next_scan IS NULL OR next_scan < now())
                    """, {'s':id, 'w': wait_until_next_scan})
                if locker.statusmessage != 'UPDATE 1':
                    continue
                share = scan_share(db_scan, id, tree_id, command)
                try:
                    share.run(proto, host, port)
                    scanners.append(share)
                except BaseException, e:
                    share.handle_exception(e)
        if len(scanners) == 0:
            break
        updating_share = None
        for scanner in tuple(scanners):
            try:
                if scanner.check_finished() and updating_share is None:
                    updating_share = scanner
            except BaseException, e:
                scanner.handle_exception(e)
                scanners.remove(scanner)
        if updating_share is not None:
            try:
                updating_share.update_db()
            except BaseException, e:
                updating_share.handle_exception(e)
            scanners.remove(updating_share)
        elif len(scanners) == max_scanners:
            time.sleep(wait_scanners_sleep)

