# webuguu.search.views - search view for django framework
#
# Copyright 2010, savrus
# Read the COPYING file in the root of the source tree.
#

from django.utils.http import urlencode, urlquote
from django.shortcuts import render_to_response
from django.core.urlresolvers import reverse
import string
import re
from webuguu.common import connectdb, offset_prepare, protocol_prepare, vfs_items_per_page, search_items_per_page, usertypes, known_filetypes, known_protocols

# for types other than recognizable by scanner
qopt_type = {
    'dir':      "(files.sharedir_id != 0)"
}

# for ordering query option 
qopt_order = {
    'avl':      "shares.state",
    'avl.d':    "shares.state DESC",
    'scan':     "shares.last_scan DESC",
    'scan.d':   "shares.last_scan",
    'uptime':   "shares.last_state_change",
    'uptime.d': "shares.last_state_change DESC",
    'proto':    "shares.protocol",
    'proto.d':  "shares.protocol DESC",
    'net':      "shares.network",
    'net.d':    "shares.network DESC",
    'host':     "shares.hostname",
    'host.d':   "shares.hostname DESC",
    'size':     "files.size DESC",
    'size.d':   "files.size",
    'name':     "filenames.name",
    'name.d':   "filenames.name DESC",
    'type':     "filenames.type DESC, (files.sharedir_id != 0) DESC",
    'type.d':   "filenames.type, (files.sharedir_id != 0)",
    'sharesize':     "shares.size DESC",
    'sharesize.d':   "shares.size",
}

qopt_match = {
    'name': "filenames.tsname @@ to_tsquery('uguu',%(query)s)",
    'full': "paths.tspath || filenames.tsname @@ to_tsquery('uguu',%(query)s)",
    'path': "paths.tspath  @@ to_tsquery('uguu',%(query)s)",
    'exact': "filenames.name = %(equery)s",
}

class QueryParser:
    def size2byte(self, size):
        sizenotatios = {'b':1, 'k':2**10, 'm':2**20, 'g':2**30, 't':2**40,
                        'kb':2**10, 'mb':2**20, 'gb':2**30, 'tb':2**40}
        m =  re.match(r'(?u)(\d+(?:\.\d+)?)(\w+)', size, re.UNICODE)
        if m == None:
            self.error += "Bad size argument: '%s'.\n" % size
            return 0
        m = m.groups()
        s = float(m[0])
        if m[1]:
            s *= sizenotatios.get(string.lower(m[1]), 1)
        return int(s)
    def parse_option_match_full(self):
        self.sqlcount_joinpath = True
    def parse_option_match_path(self):
        self.sqlcount_joinpath = True
    def parse_option_match_name(self):
        pass
    def parse_option_match_exact(self):
        pass
    def parse_option_match(self, option, arg):
        matches = {
            'name': self.parse_option_match_name,
            'full': self.parse_option_match_full,
            'path': self.parse_option_match_full,
            'exact': self.parse_option_match_exact,
        }
        if arg in matches.keys():
            self.sqltsquery = qopt_match[arg]
            matches[arg]()
        else:
            self.error += "Unsupported match argument: '%s'.\n" % arg
    def parse_option_type(self, option, arg):
        conds = []
        common = []
        for t in string.split(arg, ","):
            if qopt_type.get(t):
                conds.append(qopt_type[t])
            elif t in known_filetypes:
                common.append(t)
            else:
                self.error += "Unknown type option argument: '%s'.\n" % t
        if len(common) > 0:
            sqlcommon = "filenames.type IN %%(%s)s" % option
            conds.append(sqlcommon)
            self.options[option] = tuple(common)
        self.sqlcond.append("(" + string.join(conds, " OR ") + ")")
    def parse_option_forsize(self, option, arg, direction):
        self.sqlcond.append("files.size %s %%(%s)s" % (direction, option))
        self.options[option] = self.size2byte(arg)
    def parse_option_min(self, option, arg):
        self.parse_option_forsize(option, arg, ">=")
    def parse_option_max(self, option, arg):
        self.parse_option_forsize(option, arg, "<=")
    def parse_option_forshare(self, option, args, column):
        self.sqlcond.append("shares.%s IN %%(%s)s" % (column, option))
        self.options[option] = tuple(args)
        self.sqlcount_joinshares = True
    def parse_option_forshare_check(self, option, arg, column, known, optstr):
        args = []
        for x in string.split(arg, ','):
            if x in known:
                args.append(x)
            else:
                self.error += "Unknown %s '%s'.\n" % (optstr, x)
        self.parse_option_forshare(option, args, column)
    def parse_option_host(self, option, arg):
        self.parse_option_forshare(option, string.split(arg, ','), "hostname")
    def parse_option_net(self, option, arg):
        self.parse_option_forshare(option, string.split(arg, ','), "network")
    def parse_option_port(self, option, arg):
        args = [int(x) for x in string.split(arg, ',')]
        self.parse_option_forshare(option, args, "port")
    def parse_option_proto(self, option, arg):
        self.parse_option_forshare_check(option, arg, "protocol",
            known_protocols, "protocol")
    def parse_option_avl(self, option, arg):
        self.parse_option_forshare_check(option, arg, "state",
            ('online', 'offline'), "availability")
    def parse_option_order(self, option, arg):
        orders = []
        for x in string.split(arg, ","):
            if qopt_order.get(x):
                orders.append(qopt_order[x])
            else:
                self.error += "Unknown sorting parameter: '%s'.\n" % x
        orders = string.join(orders, ",")
        self.order = orders
    def parse_option_onlyonce_plug(self, option, arg):
        self.error += "Query option '%s' appears more than once.\n" % option
    def __init__(self, query):
        self.options = dict()
        self.order = "shares.state"
        self.error = ""
        self.sqltsquery = "filenames.tsname @@ to_tsquery('uguu',%(query)s)"
        self.userquery = query
        self.sqlcond = []
        self.sqlcount_joinpath = False;
        self.sqlcount_joinshares = False;
        qext = {
            'type':  self.parse_option_type,
            'max':   self.parse_option_max,
            'min':   self.parse_option_min,
            'match':  self.parse_option_match,
            'host':  self.parse_option_host,
            'proto': self.parse_option_proto,
            'port':  self.parse_option_port,
            'net':   self.parse_option_net,
            'avl':   self.parse_option_avl,
            'order': self.parse_option_order,
        }
        qext_executed = dict()
        words = []
        for w in re.findall(r'(?u)(\w+)(:(?:\w|\.|\,)*)?', query, re.UNICODE):
            if w[1] == "":
                ## prefix search in postgres 8.4, for postgres 8.3 use
                #words.append(w[0])
                words.append(w[0] + ":*")
            elif qext.get(w[0]):
                arg = w[1][1:]
                if arg != "":
                    qext[w[0]](w[0],arg)
                    qext[w[0]] = self.parse_option_onlyonce_plug
                else:
                    self.error += "No arguments for query option '%s'.\n" % w[0]
            else:
                self.error += "Unknown query option: '%s'.\n" % w[0]
        self.options['query'] = string.join(words, " & ")
        equery = re.search(r'(?u)(?P<equery>.*) \w+:', query, re.UNICODE)
        self.options['equery'] = equery.group('equery') if equery else "NULL"
        ## if you want to allow empty queries...
        #if len(words) > 0:
        #    self.sqlcond.append(self.sqltsquery)
        self.sqlcond.append(self.sqltsquery)
        if len(self.sqlcond) > 0:
            self.sqlquery = "WHERE " + string.join(self.sqlcond, " AND ")
        else:
            self.sqlquery = ""
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
    def getoptions(self):
        return self.options
    def geterror(self):
        return self.error

def do_search(request, index, searchform):
    try:
        query = request.GET['q']
    except:
        return render_to_response(index,
            {'form': searchform, 'types': usertypes})
    type = request.GET.get('t', "")
    types = []
    for t in usertypes:
        nt = dict(t)
        nt['selected'] = 'selected="selected"' if nt['value'] == type else ""
        types.append(nt)
    try:
        db = connectdb()
    except:
        return render_to_response('search/error.html',
            {'form': searchform, 'types': types, 'query': query,
             'error':"Unable to connect to the database."})
    cursor = db.cursor()
    parsedq = QueryParser(query + " " + type)
    if parsedq.geterror() != "":
        return render_to_response('search/error.html',
            {'form': searchform, 'types': types, 'query': query,
             'error': parsedq.geterror()})
    sqlcount = cursor.mogrify("""
        SELECT count(*) as count
        FROM filenames
        JOIN files on (filenames.filename_id = files.filename_id)
        """ + parsedq.sqlcount(), parsedq.getoptions())
    cursor.execute(sqlcount)
    items = int(cursor.fetchone()['count'])
    if items == 0:
        return render_to_response('search/error.html',
            {'form': searchform, 'types': types, 'query': query,
             'error':"Sorry, nothing found."})
    offset, gobar = offset_prepare(request, items, search_items_per_page)
    parsedq.setoption("offset", offset)
    parsedq.setoption("limit", search_items_per_page)
    sqlquery = cursor.mogrify("""
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
        """, parsedq.getoptions())
    cursor.execute(sqlquery)
    res = cursor.fetchall()
    result = []
    for row in res:
        newrow = dict()
        utfpath = unicode(row['path'], "utf-8")
        utffile = unicode(row['filename'], "utf-8")
        urlpath = "/" + utfpath if utfpath != "" else ""
        urlhost = row['hostname']
        urlhost += ":" + str(row['port']) if row['port'] != 0 else ""
        urlproto = protocol_prepare(request, row['protocol'])
        viewargs = [row['protocol'], row['hostname'], row['port']]
        if utfpath != u"":
            viewargs.append(utfpath)
        vfs = urlquote(reverse('webuguu.vfs.views.share', args=viewargs))
        vfs_offset = int(row['fileid']) / vfs_items_per_page
        newrow['pathlink'] = vfs + "?" + urlencode(dict(
            [('s', row['share_id']), ('p', row['path_id'])] +
            ([('o', vfs_offset)] if vfs_offset > 0 else []) ))
        newrow['filename'] = utffile
        if row['dirid'] > 0:
            newrow['type'] = "<dir>"
            newrow['filelink'] = vfs + utffile + "/?" + \
                urlencode({'s': row['share_id'], 'p': row['dirid']})
        else:
            newrow['type'] = ""
            newrow['filelink'] = urlproto + urlhost \
                + urlquote(urlpath + "/" + utffile)
        newrow['path'] = row['protocol'] + "://" + urlhost + urlpath
        newrow['size'] = row['size']
        newrow['state'] = row['state']
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
         'gobar': gobar,
         'sqlcount': sqlcount,
         'sqlquery': sqlquery,
         })

def search(request):
    return do_search(request, "search/index.html", "search/searchform.html")

def light(request):
    return do_search(request, "search/lightindex.html", "search/lightform.html")

