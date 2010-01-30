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
import re
from webuguu.vfs.views import vfs_items_per_page

db_host = "localhost"
db_user = "postgres"
db_password = ""
db_database = "uguu"

usertypes = (
    {'value':'',                        'text':'All'},
    {'value':'type:video min:300Mb',    'text':'Films'},
    {'value':'type:video max:400Mb',    'text':'Clips'},
    {'value':'type:audio',              'text':'Audio'},
    {'value':'type:archive',            'text':'Archives'},
    {'value':'type:directory',          'text':'Directories'}
)

conditions = {
    'video': " AND filenames.type = %(type)s",
    'audio': " AND filenames.type = %(type)s",
    'archive': " AND filenames.type = %(type)s",
    'directory': " AND files.sharedir_id > 0"
}

def connectdb():
    return psycopg2.connect(
        "host='{h}' user='{u}' " \
        "password='{p}' dbname='{d}'".format(
            h=db_host, u=db_user, p=db_password, d=db_database),
        connection_factory=DictConnection)

def size2byte(size):
    sizenotatios = {'b':1, 'kb':1024, 'mb':1024*1024, 'gb':1024*1024*1024}
    m =  re.match(r'(\d+)(\w+)', size).groups()
    s = int(m[0])
    if m[1]:
        s *= sizenotatios.get(string.lower(m[1]), 1)
    return s

class QueryParser:
    def __init__(self, query):
        self.options = dict()
        self.options['query'] = ""
        for w in re.findall(r'(\w+)(:(?:\w|\d)+)?', query):
            if w[1] == "":
                if self.options['query'] != "":
                    self.options['query'] += " & "
                self.options['query'] += w[0] + ":*"
            elif w[0] in ['type', 'max', 'min', 'full']:
                self.options[w[0]] = w[1][1:]
    def setoption(self, opt, val):
        self.options[opt] = val
    def getquery(self):
        return self.query
    def sqlwhere(self):
        str = "WHERE"
        fullpath = self.options.pop("full","")
        if fullpath != "":
            str += " paths.tspath ||"
        str += " filenames.tsname @@ to_tsquery('uguu',%(query)s)"
        type = self.options.get("type", "")
        if type != "":
            str += conditions[type]
        max = self.options.get("max")
        if max != None:
            str += "AND files.size < %(max)s"
            self.options['max'] = size2byte(max)
        min = self.options.get("min")
        if min != None:
            str += "AND files.size > %(min)s"
            self.options['min'] = size2byte(min)
        return str
    def sqlsubs(self):
        return self.options 


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
    type = request.GET.get('t', "")
    types = []
    for t in usertypes:
        nt = dict(t)
        nt['selected'] = 'selected="selected"' if nt['value'] == type else ""
        types.append(nt)
    parsedq = QueryParser(query + " " + type)
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
        """ + parsedq.sqlwhere() + """
        ORDER BY files.share_id, files.sharepath_id, files.pathfile_id
        """, parsedq.sqlsubs())
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

