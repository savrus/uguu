# webuguu.search.views - search view for django framework
#
# Copyright 2010, savrus
# Read the COPYING file in the root of the source tree.
#

import psycopg2
from psycopg2.extras import DictConnection
from django.http import HttpResponse
from django.shortcuts import render_to_response
import string

db_host = "localhost"
db_user = "postgres"
db_password = ""
db_database = "uguu"

def search(request):
    try:
        query = request.GET['q']
    except:
        return render_to_response('search/index.html')
    try:
        db = psycopg2.connect(
            "host='{h}' user='{u}' " \
            "password='{p}' dbname='{d}'".format(
                h=db_host, u=db_user, p=db_password, d=db_database),
            connection_factory=DictConnection)
    except:
        return HttpResponse("Unable to connect to the database.")
    cursor = db.cursor()
    cursor.execute("""
        SELECT protocol, hostname,
            paths.path AS path, files.sharedir_id AS dirid,
            filenames.name AS filename, files.size AS size, port
        FROM filenames
        JOIN files ON (filenames.filename_id = files.filename_id)
        JOIN paths ON (files.share_id = paths.share_id
            AND files.sharepath_id = paths.sharepath_id)
        JOIN shares ON (files.share_id = shares.share_id)
        WHERE filenames.name like %(q)s
        ORDER BY files.share_id, files.sharepath_id, files.pathfile_id
        """, {'q': "%" + query + "%"})
    if cursor.rowcount == 0:
        return render_to_response('search/noresults.html')
    else:
        res = cursor.fetchall()
        result = []
        for row in res:
            newrow = row.copy()
            del row
            if newrow['path'] != "":
                newrow['urlpath'] = "/" + newrow['path']
            else:
                newrow['urlpath'] = ""
            if newrow['port'] != 0:
                newrow['urlhost'] = newrow['hostname'] + ":" \
                                    + str(newrow['port'])
            else:
                newrow['urlhost'] = newrow['hostname']
            ##change 'smb' to 'file' here
            #if newrow['protocol'] == "smb":
            #    newrow['urlproto'] = "file"
            #else:
            #    newrow['urlproto'] = newrow['protocol']
            newrow['urlproto'] = newrow['protocol']
            result.append(newrow)
        del res
        return render_to_response('search/results.html',
            {'results': result})

