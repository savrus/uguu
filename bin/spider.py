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
import subprocess
from subprocess import PIPE, STDOUT

db_host = "localhost"
db_user = "postgres"
db_password = ""
db_database = "uguu"

path="/home/savrus/devel/uguu/scanners/smbscan"

def scan_line(cursor, share, line):
    if line[0] == "0":
        try:
            l, id, path = string.split(s=line, maxsplit=2)
        except:
            l, id = string.split(s=line, maxsplit=2)
            path = ""
        id = int(id)
        cursor.execute("""
            INSERT INTO paths (share_id, sharepath_id, path)
            VALUES (%(s)s, %(id)s, %(p)s)
            """, {'s':share, 'id':id, 'p':path})
    else:
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
            cursor.execute("""
                UPDATE paths SET parent_id = %(p)s,
                                 items = %(i)s,
                                 size = %(sz)s
                WHERE share_id = %(s)s AND sharepath_id = %(d)s
                """, {'p':path, 'i':items, 'sz':size, 's':share, 'd':dirid})
            if path == 0:
                cursor.execute("""
                    UPDATE shares SET size = %(sz)s
                    WHERE share_id = %(s)s
                    """, {'sz':size, 's':share})
        cursor.execute("SELECT filename_id FROM filenames WHERE name = %(n)s",
            {'n':name})
        try:
            filename, = cursor.fetchone()
        except:
            cursor.execute("INSERT INTO filenames (name) VALUES (%(n)s)",
                {'n':name})
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
        db.commit()
        print "Scanning", host, "succeeded"


try:
    db = psycopg2.connect(
        "host='{host}' user='{user}' " \
        "password='{password}' dbname='{dbname}'".format(
            host=db_host, user=db_user,
            password=db_password, dbname=db_database)
        )
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


