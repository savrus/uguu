# webuguu.search.views - search view for django framework
#
# Copyright 2010, savrus
# Read the COPYING file in the root of the source tree.
#

import psycopg2
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
                h=db_host, u=db_user, p=db_password, d=db_database))
    except:
        return HttpResponse("Unable to connect to the database.")
    cursor = db.cursor()
    cursor.execute("""
        SELECT proto.name, host.name,
            path.path, spath.path, filename.name
        FROM filename
        JOIN file ON (filename.filename_id = file.filename_id)
        LEFT OUTER JOIN path ON (file.share_id = path.share_id
            AND file.parent_within_share_id = path.path_within_share_id)
        LEFT OUTER JOIN path AS spath ON (file.share_id = spath.share_id
            AND file.path_within_share_id = spath.path_within_share_id)
        JOIN share ON (file.share_id = share.share_id)
        JOIN host ON (share.host_id = host.host_id)
        JOIN sharetype ON (share.sharetype_id = sharetype.sharetype_id)
        JOIN proto ON (sharetype.proto_id = proto.proto_id)
        WHERE filename.name like %(q)s
        ORDER BY file.share_id, file.parent_within_share_id,
            file.file_within_path_id
        """, {'q': "%" + query + "%"})
    if cursor.rowcount == 0:
        return render_to_response('search/noresults.html')
    else:
        return render_to_response('search/results.html',
            {'results': cursor.fetchall()})

