#!/usr/bin/python

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
            INSERT INTO path (share_id, path_within_share_id, parent_id, path)
            VALUES (%(s)s, %(pws)s, %(p)s, %(pn)s)
            """, {'s':share, 'pws':path, 'p':parent, 'pn':name})
        filetype = 0
    else:
        filetype = 1
    name = name[string.rfind(name, "/") + 1 :]
    cursor.execute("SELECT filename_id FROM filename WHERE name = %(n)s",
        {'n':name})
    try:
        filename, = cursor.fetchone()
    except:
        cursor.execute("INSERT INTO filename (name) VALUES (%(n)s)",
            {'n':name})
        cursor.execute("SELECT * FROM lastval()")
        filename, = cursor.fetchone()
    cursor.execute("""
        INSERT INTO file (share_id,
                          path_within_share_id,
                          file_within_path_id,
                          size,
                          filename_id,
                          file_type)
        VALUES (%(s)s, %(pws)s, %(fwp)s, %(sz)s, %(fn)s, %(ft)s)
        """, {'s':share, 'pws':parent, 'fwp':file, 'sz':size,
              'fn':filename, 'ft':filetype})

def scan_share(db, share_id, ip, command):
    cursor = db.cursor()
    cursor.execute("DELETE FROM file WHERE share_id = %(id)s", {'id':share_id})
    cursor.execute("DELETE FROM path WHERE share_id = %(id)s", {'id':share_id})
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
    SELECT share_id, ip_address, scan_command
    FROM share
    LEFT JOIN sharetype ON share.sharetype_id = sharetype.sharetype_id
    LEFT JOIN host ON share.host_id = host.host_id
    """)
for id, ip, command in shares.fetchall():
    scan_share(db, id, ip, command)


