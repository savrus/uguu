# webuguu.search.views - search view for django framework
#
# Copyright 2010, savrus
# Read the COPYING file in the root of the source tree.
#

from django.utils.http import urlencode
from django.shortcuts import render_to_response
from django.core.urlresolvers import reverse
import string
import re
from webuguu.common import connectdb, generate_go_bar, vfs_items_per_page, search_items_per_page, usertypes

# for types other than recognizable by scanner
conditions = {
    'dir':      " AND files.sharedir_id > 0"
}

# orders supported by 'order' query extension
order2query = {
    'state': "shares.state DESC",
    'host': "shares.network, shares.netshare_id",
    'size': "files.size DESC"
}

def size2byte(size):
    sizenotatios = {'b':1, 'k':1024, 'm':1024*1024, 'g':1024*1024*1024,
                    'kb':1024, 'mb':1024*1024, 'gb':1024*1024*1024}
    m =  re.match(r'(\d+)(\w+)', size).groups()
    s = int(m[0])
    if m[1]:
        s *= sizenotatios.get(string.lower(m[1]), 1)
    return s

class QueryParser:
    def __init__(self, query):
        self.options = dict()
        self.options['query'] = ""
        self.order = "shares.state DESC"
        for w in re.findall(r'(?u)(\w+)(:(?:\w|\.|\,)*)?', query):
            if w[1] == "":
                if self.options['query'] != "":
                    self.options['query'] += " & "
                self.options['query'] += w[0] + ":*"
            elif w[0] in ['type', 'max', 'min', 'full', 'host', 'proto', 'port', 'net', 'order']:
                self.options[w[0]] = w[1][1:]
        self.sqlquery = "WHERE"
        fullpath = self.options.get("full","")
        if fullpath != "":
            self.sqlquery += " paths.tspath ||"
        self.sqlquery += " filenames.tsname @@ to_tsquery('uguu',%(query)s)"
        type = self.options.get("type", "")
        if type != "":
            if conditions.get(type, "") != "":
                self.sqlquery += conditions[type]
            else:
                self.sqlquery += " AND filenames.type = %(type)s"
        max = self.options.get("max")
        if max != None:
            self.sqlquery += " AND files.size < %(max)s"
            self.options['max'] = size2byte(max)
        min = self.options.get("min")
        if min != None:
            self.sqlquery += " AND files.size > %(min)s"
            self.options['min'] = size2byte(min)
        host = self.options.get("host", "")
        if host != "":
            self.sqlquery += " AND shares.hostname = %(host)s"
        proto = self.options.get("proto", "")
        if proto != "":
            self.sqlquery += " AND shares.protocol = %(proto)s"
        port = self.options.get("port", "")
        if port != "":
            self.sqlquery += " AND shares.port = %(port)s"
        net = self.options.get("net", "")
        if net != "":
            self.sqlquery += " AND shares.network = %(net)s"
        order = self.options.get("order", "")
        if order != "":
            orders = [order2query.get(x) for x in string.split(order, ",")]
            orders = string.join(filter(lambda x: x, orders), ",")
            if orders != "":
                self.order = orders
    def setoption(self, opt, val):
        self.options[opt] = val
    def sqlwhere(self):
        return self.sqlquery
    def sqlorder(self):
        return self.order
    def sqlcount(self):
        str = ""
        if self.options.get("full", "") != "":
            str += """
                JOIN paths ON (files.share_id = paths.share_id
                    AND files.sharepath_id = paths.sharepath_id)
                """
        if self.options.get("net", "") != "" or \
           self.options.get("proto", "") != "" or \
           self.options.get("host", "") != "" or \
           self.options.get("port", "") != "":
            str += """
                JOIN shares ON (files.share_id = shares.share_id)
                """
        return str + self.sqlquery
    def sqlsubs(self):
        return self.options


def do_search(request, index, searchform):
    try:
        query = request.GET['q']
    except:
        return render_to_response(index,
            {'form': searchform, 'types': usertypes})
    try:
        db = connectdb()
    except:
        return render_to_response('search/error.html',
            {'error':"Unable to connect to the database."})
    cursor = db.cursor()
    type = request.GET.get('t', "")
    types = []
    for t in usertypes:
        nt = dict(t)
        nt['selected'] = 'selected="selected"' if nt['value'] == type else ""
        types.append(nt)
    parsedq = QueryParser(query + " " + type)
    cursor.execute("""
        SELECT count(*) as count
        FROM filenames
        JOIN files on (filenames.filename_id = files.filename_id)
        """ + parsedq.sqlcount(), parsedq.sqlsubs())
    items = int(cursor.fetchone()['count'])
    page_offset = int(request.GET.get('o', 0))
    offset = page_offset * search_items_per_page
    gobar = generate_go_bar(items, page_offset)
    parsedq.setoption("offset", offset)
    parsedq.setoption("limit", search_items_per_page)
    cursor.execute("""
        SELECT protocol, hostname,
            paths.path AS path, files.sharedir_id AS dirid,
            filenames.name AS filename, files.size AS size, port,
            shares.share_id, paths.sharepath_id as path_id,
            files.pathfile_id as fileid, shares.state
        FROM filenames
        JOIN files ON (filenames.filename_id = files.filename_id)
        JOIN paths ON (files.share_id = paths.share_id
            AND files.sharepath_id = paths.sharepath_id)
        JOIN shares ON (files.share_id = shares.share_id)
        """ + parsedq.sqlwhere() + """
        ORDER BY """ + parsedq.sqlorder() +
            """, files.share_id, files.sharepath_id, files.pathfile_id
        OFFSET %(offset)s LIMIT %(limit)s
        """, parsedq.sqlsubs())
    if cursor.rowcount == 0:
        return render_to_response('search/error.html',
            {'form': searchform, 'types': types, 'query': query,
             'error':"Sorry, nothing found."})
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
            vfs_offset = int(row['fileid']) / vfs_items_per_page
            newrow['pathlink'] = vfs + "?" + urlencode(dict(
                [('s', row['share_id']), ('p', row['path_id'])] +
                ([('o', vfs_offset)] if vfs_offset > 0 else []) ))
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
            newrow['state'] = "online" if row['state'] else "offline"
            result.append(newrow)
            del row
        del res
        fastselflink = "./?" + urlencode(dict([('q', query), ('t', type)]))
        return render_to_response('search/results.html',
            {'form': searchform,
             'query': query,
             'types': types,
             'results': result,
             'offset': offset,
             'fastself': fastselflink,
             'gobar': gobar
             })

def search(request):
    return do_search(request, "search/index.html", "search/searchform.html")

def light(request):
    return do_search(request, "search/lightindex.html", "search/lightform.html")

