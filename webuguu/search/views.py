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
    'dir':      "files.sharedir_id > 0"
}


class QueryParser:
    def size2byte(self, size):
        sizenotatios = {'b':1, 'k':1024, 'm':1024*1024, 'g':1024*1024*1024,
                        'kb':1024, 'mb':1024*1024, 'gb':1024*1024*1024,
                        't':1024*1024*1024*1024, 'tb':1024*1024*1024*1024}
        m =  re.match(r'(?u)(\d+)(\w+)', size, re.UNICODE)
        if m == None:
            self.error = "Bad size option '" + size + "'.\n"
            return 0
        m = m.groups()
        s = int(m[0])
        if m[1]:
            s *= sizenotatios.get(string.lower(m[1]), 1)
        return s
    def parse_option_full(self, option, arg):
        self.sqltsquery = " paths.tspath ||" + self.sqltsquery
        self.sqlcount_joinpath = True
    def parse_option_type(self, option, arg):
        conds = []
        common = []
        for t in string.split(arg, ","):
            if conditions.get(t):
                conds.append(conditions[t])
            else:
                common.append(t)
        if len(common) > 0:
            sqlcommon = "filenames.type IN %(" + option +")s"
            conds.append(sqlcommon)
            self.options[option] = tuple(common)
        self.sqlcond.append("(" + string.join(conds, " OR ") + ")")
    def parse_option_forsize(self, option, arg):
        forsize = {'min':">", 'max':"<"}
        self.sqlcond.append("files.size " + forsize[option] +" %(" + option +")s")
        self.options[option] = self.size2byte(arg)
    def parse_option_forshare(self, option, arg):
        forshare = {'proto':"protocol", 'host':"hostname",
                    'port':"port", 'net':"network"}
        args = string.split(arg, ",")
        if option == "port":
            args = [int(x) for x in args]
        if forshare.get(option) == None:
            self.error += "Not aware of query extension: '" + option + "'.\n"
            return
        self.sqlcond.append("shares." + forshare[option] + " IN %(" + option +")s")
        self.options[option] = tuple(args)
        self.sqlcount_joinshares = True
    def parse_option_order(self, option, arg):
        order2query = {
            'online': "shares.state DESC",
            'online.d': "shares.state",
            'host': "shares.network, shares.hostname",
            'host.d': "shares.network DESC, shares.hostname DESC",
            'size': "files.size DESC",
            'size.d': "files.size"}
        orders = [order2query.get(x) for x in string.split(arg, ",")]
        orders = string.join(filter(lambda x: x, orders), ",")
        if orders != "":
            self.order = orders
    def __init__(self, query):
        self.options = dict()
        self.order = "shares.state DESC"
        self.error = ""
        self.sqltsquery = " filenames.tsname @@ to_tsquery('uguu',%(query)s)"
        self.sqlcond = []
        self.sqlcount_joinpath = False;
        self.sqlcount_joinshares = False;
        qext = {
            'type':  self.parse_option_type,
            'max':   self.parse_option_forsize,
            'min':   self.parse_option_forsize,
            'full':  self.parse_option_full,
            'host':  self.parse_option_forshare,
            'proto': self.parse_option_forshare,
            'port':  self.parse_option_forshare,
            'net':   self.parse_option_forshare,
            'sort':  self.parse_option_order,
        }
        qext_executed = dict()
        words = []
        for w in re.findall(r'(?u)(\w+)(:(?:\w|\.|\,)*)?', query, re.UNICODE):
            if w[1] == "":
                words.append(w[0] + ":*")
            elif w[0] in qext.keys():
                if qext_executed.get(w[0]):
                    self.error += "Query extension '" + w[0] + "' appears more than once.\n"
                    continue
                arg = w[1][1:]
                if arg != "":
                    qext[w[0]](w[0],arg)
                    qext_executed[w[0]] = True
                else:
                    self.error += "No arguments for query extension '" + w[0] + "'.\n"
            else:
                self.error += "Unknown query extension: '" + w[0] + "'.\n"
        self.options['query'] = string.join(words, " & ")
        self.sqlcond.append(self.sqltsquery)
        self.sqlquery = "WHERE " + string.join(self.sqlcond, " AND ")
    def setoption(self, opt, val):
        self.options[opt] = val
    def sqlwhere(self):
        return self.sqlquery
    def sqlorder(self):
        return self.order
    def sqlcount(self):
        str = ""
        if self.sqlcount_joinpath:
            str += """
                JOIN paths ON (files.share_id = paths.share_id
                    AND files.sharepath_id = paths.sharepath_id)
                """
        if self.sqlcount_joinshares:
            str += """
                JOIN shares ON (files.share_id = shares.share_id)
                """
        return str + self.sqlquery
    def sqlsubs(self):
        return self.options
    def geterror(self):
        return self.error

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
            {'form': searchform, 'types': types, 'query': query,
             'error':"Unable to connect to the database."})
    cursor = db.cursor()
    type = request.GET.get('t', "")
    types = []
    for t in usertypes:
        nt = dict(t)
        nt['selected'] = 'selected="selected"' if nt['value'] == type else ""
        types.append(nt)
    parsedq = QueryParser(query + " " + type)
    if parsedq.geterror() != "":
        return render_to_response('search/error.html',
            {'form': searchform, 'types': types, 'query': query,
             'error': parsedq.geterror()})
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

