# webuguu.search.views - search view for django framework
#
# Copyright 2010, savrus
# Read the COPYING file in the root of the source tree.
#

from django.http import HttpResponse
from django.utils.http import urlencode, urlquote
from django.shortcuts import render_to_response
from django.core.urlresolvers import reverse
from django.utils import feedgenerator
from django.template import Context, Template
import string
import re
import time
from webuguu.common import connectdb, offset_prepare, protocol_prepare, vfs_items_per_page, search_items_per_page, usertypes, known_filetypes, known_protocols, debug_virtual_host, rss_items, rss_feed_add_item

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
    'name':     "files.name",
    'name.d':   "files.name DESC",
    'type':     "files.type",
    'type.d':   "files.type DESC",
    'sharesize':     "shares.size DESC",
    'sharesize.d':   "shares.size",
}

qopt_match = {
    'name': "files.tsname @@  to_tsquery('uguu', %(query)s)",
    'full': "files.tspath || files.tsname @@ to_tsquery('uguu',%(query)s)",
    'name.p': "files.tsname @@  to_tsquery('uguu', %(query)s)",
    'full.p': "files.tspath || files.tsname @@ to_tsquery('uguu',%(query)s)",
    'exact': "lower(files.name) = lower(%(equery)s)",
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
        pass
    def parse_option_match_name(self):
        pass
    def parse_option_match_full_prefix(self):
        self.sql_full_search_prefix = True
    def parse_option_match_name_prefix(self):
        self.sql_full_search_prefix = True
    def parse_option_match_exact(self):
        pass
    def parse_option_match(self, option, arg):
        matches = {
            'name': self.parse_option_match_name,
            'full': self.parse_option_match_full,
            'name.p': self.parse_option_match_name_prefix,
            'full.p': self.parse_option_match_full_prefix,
            'exact': self.parse_option_match_exact,
        }
        if arg in matches.keys():
            #self.sqltsquery = qopt_match[arg]
            self.options['tsquery'] = arg
            matches[arg]()
        else:
            self.error += "Unsupported match argument: '%s'.\n" % arg
    def parse_option_type(self, option, arg):
        conds = []
        common = []
        for t in string.split(arg, ","):
            if t in known_filetypes:
                common.append(t)
            else:
                self.error += "Unknown type option argument: '%s'.\n" % t
        if len(common) > 0:
            sqlcommon = "files.type IN %%(%s)s" % option
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
    def parse_option_out_html(self):
        pass
    def parse_option_out_rss(self):
        self.options['offset'] = 0
        self.options['limit'] = rss_items
    def parse_option_out(self, option, arg):
        outs = {
            'html': self.parse_option_out_html,
            'rss':  self.parse_option_out_rss,
        }
        if arg in outs.keys():
            self.options['output'] = arg
            outs[arg]()
        else:
            self.error += "Unsupported out argument: '%s'.\n" % arg
    def parse_option_onlyonce_plug(self, option, arg):
        self.error += "Query option '%s' appears more than once.\n" % option
    def __init__(self, query):
        self.options = dict()
        self.options['tsquery'] = 'name'
        self.options['output'] = 'html'
        self.order = "shares.state, files.size DESC"
        self.error = ""
        self.userquery = query
        self.sqlcond = []
        self.sqlcount_joinpath = False
        self.sqlcount_joinshares = False
        self.sql_full_search_prefix = False
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
            'out':   self.parse_option_out,
        }
        qext_executed = dict()
        words = []
        for w in re.findall(r'(?u)(\w+)(:(?:\w|\.|\,)*)?', query, re.UNICODE):
            if w[1] == "":
                words.append(w[0])
            elif qext.get(w[0]):
                option = w[0].encode("ascii")
                arg = w[1][1:]
                if arg != "":
                    qext[option](option,arg)
                    qext[option] = self.parse_option_onlyonce_plug
                else:
                    self.error += "No arguments for query option '%s'.\n" % w[0]
            else:
                self.error += "Unknown query option: '%s'.\n" % w[0]
        self.sqltsquery = qopt_match[self.options['tsquery']]
        if self.sql_full_search_prefix:
            ## prefix search in postgres 8.4, for postgres 8.3 just remove
            words = [x + ":*" for x in words]
        self.options['query'] = string.join(words, " & ")
        equery = re.search(r'(?u)(?P<equery>[^:]*) \w+:', query, re.UNICODE)
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
        if self.options.get(opt, None) == None:
            self.options[opt] = val
    def sqlwhere(self):
        return self.sqlquery
    def sqlorder(self):
        return self.order
    def sqlcount(self):
        str = ""
        if self.sqlcount_joinshares:
            str += """
                JOIN shares USING (share_id)
                """
        return str + self.sqlquery
    def getoptions(self):
        return self.options
    def geterror(self):
        return self.error

def do_search(request, index, searchform):
    generation_started = time.time()
    sqlecho = debug_virtual_host(request)
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
        FROM files
        """ + parsedq.sqlcount(), parsedq.getoptions())
    cursor.execute(sqlcount)
    items = int(cursor.fetchone()['count'])
    if items == 0 and sqlecho == 0:
        return render_to_response('search/error.html',
            {'form': searchform, 'types': types, 'query': query,
             'error':"Sorry, nothing found."})
    offset, gobar = offset_prepare(request, items, search_items_per_page)
    parsedq.setoption("offset", offset)
    parsedq.setoption("limit", search_items_per_page)
    sqlquery = cursor.mogrify("""
        SELECT protocol, hostname, hostaddr,
            paths.path AS path, files.sharedir_id AS dirid,
            files.name AS filename, files.size AS size, port,
            shares.share_id, paths.sharepath_id as path_id,
            files.pathfile_id as fileid, shares.state
        FROM files
        JOIN paths USING (share_id, sharepath_id)
        JOIN shares USING (share_id)
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
        host = row['hostname']
        urlhost = row['hostaddr'] if row['hostaddr'] else row['hostname']
        host += ":" + str(row['port']) if row['port'] != 0 else ""
        urlhost += ":" + str(row['port']) if row['port'] != 0 else ""
        urlproto = protocol_prepare(request, row['protocol'])
        viewargs = [row['protocol'], row['hostname'], row['port']]
        if utfpath != u"":
            viewargs.append(utfpath)
        vfs = reverse('webuguu.vfs.views.share', args=viewargs)
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
        newrow['path'] = row['protocol'] + "://" + host + urlpath
        newrow['size'] = row['size']
        newrow['state'] = row['state']
        result.append(newrow)
        del row
    del res
    fastselflink = "./?" + urlencode(dict([('q', query), ('t', type)]))
    if parsedq.getoptions()['output'] == "html":
        return render_to_response('search/results.html',
            {'form': searchform,
             'query': query,
             'types': types,
             'results': result,
             'offset': offset,
             'fastself': fastselflink,
             'gobar': gobar,
             'sqlecho': sqlecho,
             'sqlcount': sqlcount,
             'sqlquery': sqlquery,
             'gentime': time.time() - generation_started,
             })
    elif parsedq.getoptions()['output'] == "rss":
        feed = feedgenerator.Rss201rev2Feed(
            title = u"Search Results",
            link = reverse('webuguu.search.views.search'),
            description = u"Results of a query: " + query,
            language=u"en")
        curdescs = []
        curname = ""
        for file in result:
            if file['filename'] != curname:
                if curname != "":
                    rss_feed_add_item(request, feed, curname, string.join(curdescs, "<br>"))
                curdescs = []
                curname = file['filename']
            tmpl = Template('<a href="{{r.pathlink|iriencode}}">{{r.path}}</a>'
                + '/<a class="share" href="{{r.filelink|iriencode}}">{{r.filename}}</a>'
                + ' ({{r.size|filesizeformat}} {{r.state}})')
            ctx = Context({'r':file})
            curdescs.append(tmpl.render(ctx))
        if len(curdescs) > 0:
            rss_feed_add_item(request, feed, curname, string.join(curdescs, "<br>"))
        return HttpResponse(feed.writeString('UTF-8'))
    else:
        return render_to_response('search/error.html',
            {'form': searchform, 'types': types, 'query': query,
             'error':"Unsupported output."})
        

def search(request):
    return do_search(request, "search/index.html", "search/searchform.html")

def light(request):
    return do_search(request, "search/lightindex.html", "search/lightform.html")

