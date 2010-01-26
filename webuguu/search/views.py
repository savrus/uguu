# webuguu.search.views - search view for django framework
#
# Copyright 2010, savrus
# Read the COPYING file in the root of the source tree.
#

import psycopg2
from psycopg2.extras import DictConnection
from django.http import HttpResponse
from django.utils.http import urlencode
from django.shortcuts import render_to_response
from django.core.urlresolvers import reverse
import string
from webuguu.vfs.views import vfs_items_per_page

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
        nt['selected'] = 'selected="selected"' if nt['value'] == type else ""
        types.append(nt)
    cursor.execute("""
        SELECT protocol, hostname,
            paths.path AS path, files.sharedir_id AS dirid,
            filenames.name AS filename, files.size AS size, port,
            shares.share_id, paths.sharepath_id as path_id,
            files.pathfile_id as fileid
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
            newrow = dict()
            urlpath = "/" + row['path'] if row['path'] != "" else ""
            urlhost = row['hostname']
            urlhost += ":" + str(row['port']) if row['port'] != 0 else ""
            ##change 'smb' to 'file' here
            #if row['protocol'] == "smb":
            #    urlproto = "file"
            #else:
            #    urlproto = row['protocol']
            urlproto = row['protocol']
            viewargs = [row['protocol'], row['hostname'], row['port']]
            if row['path'] != "":
                viewargs.append(row['path'])
            vfs = reverse('webuguu.vfs.views.share', args=viewargs)
            offset = int(row['fileid']) / vfs_items_per_page
            newrow['pathlink'] = vfs + "?" + urlencode(dict(
                [('s', row['share_id']), ('p', row['path_id'])] +
                ([('o', offset)] if offset > 0 else []) ))
            newrow['filename'] = row['filename']
            if row['dirid'] > 0:
                newrow['type'] = "<dir>"
                newrow['filelink'] = vfs + newrow['filename'] + "/?" + \
                    urlencode({'s': row['share_id'], 'p': row['dirid']})
            else:
                newrow['type'] = ""
                newrow['filelink'] = urlproto + "://" +\
                    urlhost + urlpath + "/" + newrow['filename']
            newrow['path'] = row['protocol'] + "://" + urlhost + urlpath
            newrow['size'] = row['size']
            result.append(newrow)
            del row
        del res
        return render_to_response('search/results.html',
            {'types': types, 'results': result,
             'query': query})

