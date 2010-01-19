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
    try:
        parent, path, file, size, name = string.split(s=line, maxsplit=4)
    except:
        parent, path, file, size = string.split(s=line, maxsplit=4)
        name = ""
    parent = int(parent)
    path = int(path)
    file = int(file)
    size = int(size)
    if path > 0:
        cursor.execute("""
            INSERT INTO paths (share_id, sharepath_id, parent_id, path)
            VALUES (%(s)s, %(pws)s, %(p)s, %(pn)s)
            """, {'s':share, 'pws':path, 'p':parent, 'pn':name})
        filetype = 0
    else:
        filetype = 1
    name = name[string.rfind(name, "/") + 1 :]
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
        """, {'s':share, 'p':parent, 'f':file, 'did':path, 'sz':size,
              'fn':filename })

def scan_share(db, share_id, ip, command):
    cursor = db.cursor()
    cursor.execute("DELETE FROM files WHERE share_id = %(id)s", {'id':share_id})
    cursor.execute("DELETE FROM paths WHERE share_id = %(id)s", {'id':share_id})
    cmd = path + '/' + command + ' ' + ip + ' 2>/dev/null'
    data = subprocess.Popen(cmd, shell=True, stdin=PIPE,
                            stdout=PIPE, stderr=None, close_fds=True)
    for line in data.stdout:
        scan_line(cursor, share_id, line.strip())
    data.stdin.close()
    if data.wait() != 0:
        db.rollback()
        print "Scanning", ip, "failed"
    else:
        db.commit()
        print "Scanning", ip, "succeeded"


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
    SELECT share_id, host_addr, scan_command
    FROM shares
    LEFT JOIN scantypes ON shares.scantype_id = scantypes.scantype_id
    LEFT JOIN hosts ON shares.host_id = hosts.host_id
    """)
for id, ip, command in shares.fetchall():
    scan_share(db, id, ip, command)


