#!/usr/bin/python
#
# spider.py - high-level scanner
#
# Copyright 2010, savrus
# Read the COPYING file in the root of the source tree.
#

import psycopg2
from psycopg2.extras import DictConnection
import sys
import string
import subprocess
import re
from subprocess import PIPE, STDOUT

#TODO: locale specify the scanners output charset.
locale = "utf-8"

db_host = "localhost"
db_user = "postgres"
db_password = ""
db_database = "uguu"

def connectdb():
    return psycopg2.connect(
        "host='%(h)s' user='%(u)s' password='%(p)s' dbname='%(d)s'" \
            % {'h':db_host, 'u':db_user, 'p':db_password, 'd':db_database},
        connection_factory=DictConnection)


types = dict(
    [(x,'audio') for x in ('mp3', 'ogg', 'vaw', 'flac', 'ape')] +
    [(x,'video') for x in ('mkv', 'avi', 'mp4', 'mov')] +
    [(x,'document') for x in ('txt', 'doc', 'xls', 'rtf')] +
    [(x,'archive') for x in ('bz', 'gz', 'bz2', 'tar', 'tbz', 'tgz', 'zip', 'rar', 'arj')] +
    [(x,'image') for x in ('jpg', 'jpeg', 'gif', 'png', 'bmp', 'tiff')]
)

path="/home/savrus/devel/uguu/scanners/smbscan"

def suffix(filename):
    dot = filename.rfind(".")
    if dot == -1:
        return ""
    else:
        return string.lower(filename[dot + 1:])


def tsprepare(string):
    relax = unicode(string, locale)
    relax = re.sub(r'(?u)\W', ' ', relax)
    relax = re.sub(r'(?u)([Ss])(\d+)([Ee])(\d+)',
                   '\\1\\2\\3\\4 \\2 \\4', relax)
    return relax


def scan_line(cursor, share, line):
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
            VALUES (%(s)s, %(id)s, %(p)s, %(t)s)
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
            cursor.execute("SELECT filename_id FROM filenames WHERE name = %(n)s",
                {'n':name})
            if cursor.rowcount > 0:
                filename, = cursor.fetchone()
            else:
                suf = suffix(name)
                type = types.get(suf)
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


def scan_share(db, share_id, host, command):
    cursor = db.cursor()
    cursor.execute("DELETE FROM files WHERE share_id = %(id)s", {'id':share_id})
    cursor.execute("DELETE FROM paths WHERE share_id = %(id)s", {'id':share_id})
    cmd = path + '/' + command + ' ' + host + ' 2>/dev/null'
    data = subprocess.Popen(cmd, shell=True, stdin=PIPE,
                            stdout=PIPE, stderr=None, close_fds=True)
    for line in data.stdout:
        scan_line(cursor, share_id, line.strip('\n'))
    data.stdin.close()
    if data.wait() != 0:
        db.rollback()
        print "Scanning", host, "failed"
    else:
        cursor.execute("UPDATE shares SET last_scan = now()")
        db.commit()
        print "Scanning", host, "succeeded"


try:
    db = connectdb()
except:
    print "Unable to connect to the database, exiting."
    sys.exit()
shares = db.cursor()
shares.execute("""
    SELECT share_id, hostname, scan_command
    FROM shares
    LEFT JOIN scantypes ON shares.scantype_id = scantypes.scantype_id
    """)
for id, host, command in shares.fetchall():
    scan_share(db, id, host, command)


