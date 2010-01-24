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

usertypes = (
    {'value':'all',         'text':'All'},
    {'value':'film',        'text':'Films'},
    {'value':'clip',        'text':'Clips'},
    {'value':'audio',       'text':'Audio'},
    {'value':'archive',     'text':'Archives'},
    {'value':'directory',   'text':'Directories'}
)

conditions = {
    'all': "",
    'film': "AND filenames.type = 'video' AND files.size > " + str(300 * 1024 * 1024),
    'clip': "AND filenames.type = 'video' AND files.size < " + str(350 * 1024 * 1024),
    'audio': "AND filenames.type = 'audio'",
    'archive': "AND filenames.type = 'archive'",
    'directory': "AND files.sharedir_id > 0"
}

def connectdb():
    return psycopg2.connect(
        "host='{h}' user='{u}' " \
        "password='{p}' dbname='{d}'".format(
            h=db_host, u=db_user, p=db_password, d=db_database),
        connection_factory=DictConnection)

def search(request):
    try:
        query = request.GET['q']
    except:
        return render_to_response('search/index.html',
            {'types': usertypes})
    try:
        db = connectdb()
    except:
        return HttpResponse("Unable to connect to the database.")
    cursor = db.cursor()
    type = request.GET.get('t', "all")
    types = []
    for t in usertypes:
        nt = dict(t)
        if nt['value'] == type:
            nt['selected'] = "selected"
        else:
            nt['selected'] = ""
        types.append(nt)
    cursor.execute("""
        SELECT protocol, hostname,
            paths.path AS path, files.sharedir_id AS dirid,
            filenames.name AS filename, files.size AS size, port,
            shares.share_id, paths.sharepath_id as path_id
        FROM filenames
        JOIN files ON (filenames.filename_id = files.filename_id)
        JOIN paths ON (files.share_id = paths.share_id
            AND files.sharepath_id = paths.sharepath_id)
        JOIN shares ON (files.share_id = shares.share_id)
        WHERE filenames.name like %(q)s
        """ + conditions.get(type, "") + """
        ORDER BY files.share_id, files.sharepath_id, files.pathfile_id
        """, {'q': "%" + query + "%"})
    if cursor.rowcount == 0:
        return render_to_response('search/noresults.html',
            {'types': types, 'query': query})
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
            {'types': types, 'results': result,
             'query': query})

