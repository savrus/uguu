# webuguu.vfs.views - vfs view for django framework
#
# Copyright 2010, savrus
# Read the COPYING file in the root of the source tree.
#

from django.http import HttpResponseRedirect
from django.utils.http import urlencode
from django.shortcuts import render_to_response
#from django.core.urlresolvers import reverse
#import string
import time
import datetime
from webuguu.common import connectdb, vfs_items_per_page, offset_prepare, protocol_prepare, known_protocols, hostname_prepare


def index(request):
#    generation_started = time.time()
#    try:
#        db = connectdb()
#    except:
#        return render_to_response('vfs/error.html',
#            {'error':"Unable to connect to the database."})
    return render_to_response('vfs/index.html')


def net(request):
    generation_started = time.time()
    try:
        db = connectdb()
    except:
        return render_to_response('vfs/error.html',
            {'error':"Unable to connect to the database."})
    cursor = db.cursor()
    cursor.execute("""
            SELECT network, items, online, offline, size, avg
            FROM networks
            LEFT JOIN (
                SELECT network, 
                count(*) AS items,
                sum(case when state = 'online' then 1 else 0 end) AS online,
                sum(case when state = 'offline' then 1 else 0 end) AS offline,
                sum(size) AS size,
                avg(size) AS avg
                FROM shares GROUP BY network
            ) AS nstat USING(network)
            ORDER BY network
        """)
    return render_to_response('vfs/net.html', \
        {'networks': cursor.fetchall(),
         'gentime': time.time() - generation_started,
        })

def sharelist(request, column, name, is_this_host):
    generation_started = time.time()
    try:
        db = connectdb()
    except:
        return render_to_response('vfs/error.html',
            {'error':"Unable to connect to the database."})
    cursor = db.cursor()
    try:
        order = request.GET.get('order')
        url = dict()
        url['order'] = [('order', order)] if order != None else []
    except:
        return render_to_response('vfs/error.html',
            {'error':"Wrong parameters."})
    cursor.execute("""
        SELECT 
            count(*) AS items,
            sum(case when state = 'online' then 1 else 0 end) AS online,
            sum(case when state = 'offline' then 1 else 0 end) AS offline,
            sum(size) AS size,
            avg(size) AS avg
        FROM shares WHERE %s = %%(n)s
        """ % column, {'n': name})
    try:
        listinfo = cursor.fetchone()
        items = listinfo['items']
        if items == 0:
            raise
    except:
        return render_to_response('vfs/error.html',
            {'error':"No shares within %s '%s'" % (column, name)})
    offset, gobar = offset_prepare(request, items, vfs_items_per_page)
    orders = {
        'size': "size DESC",
        'name': "state, hostname"
    }
    if not orders.get(order):
        order = 'name'
    cursor.execute("""
        SELECT share_id, state, size, network, protocol, hostname, port
        FROM shares
        WHERE %s = %%(n)s
        ORDER BY %s
        OFFSET %%(o)s LIMIT %%(l)s
        """ % (column, orders[order]), {'n':name, 'o':offset, 'l':vfs_items_per_page})
    fastselflink = "./?" + urlencode(url['order'])
    orderbar = dict()
    orderbar['nontrivial'] = (cursor.rowcount > 1)
    orderbar['order'] = "./?"
    orderbar['orders'] = [{'n': k, 's': k == order} for k in orders.keys()]
    return render_to_response("vfs/sharelist.html", \
        {'name': name,
         'ishost': is_this_host,
         'shares': cursor.fetchall(),
         'fastself': fastselflink,
         'gobar': gobar,
         'orderbar': orderbar,
         'info': listinfo,
         'gentime': time.time() - generation_started,
         'offset': offset,
         })


def network(request, network):
    return sharelist(request, "network", network, 0)


def host(request, proto, hostname):
    return sharelist(request, "hostname", hostname, 1)


def share(request, proto, hostname, port, path = ""):
    generation_started = time.time()
    try:
        db = connectdb()
    except:
        return render_to_response('vfs/error.html',
            {'error':"Unable to connect to the database."})
    if proto not in known_protocols:
        return render_to_response('vfs/error.html',
            {'error':"Unsupported protocol: '%s'" % proto})
    cursor = db.cursor()
    try:
        share_id = int(request.GET.get('s', 0))
        path_id = int(request.GET.get('p', 0))
        page_offset = int(request.GET.get('o', 0))
        order = request.GET.get('order')
        goup = int(request.GET.get('up', 0))
        url = dict()
        url['share'] = [('s', share_id)]
        url['order'] = [('order', order)] if order != None else []
    except:
        return render_to_response('vfs/error.html',
            {'error':"Wrong parameters."})
    # detect share
    if share_id != 0:
        cursor.execute("""
            SELECT tree_id, hostaddr, state, last_scan, last_state_change
            FROM shares
            WHERE share_id = %(s)s AND protocol = %(pr)s
                AND hostname = %(h)s AND port = %(p)s
            """, {'s':share_id, 'pr': proto, 'h': hostname, 'p': port})
        try:
            tree_id, hostaddr, state, scantime, changetime = cursor.fetchone()
        except:
            return HttpResponseRedirect(".")
    else:
        cursor.execute("""
            SELECT share_id, tree_id, hostaddr,
                state, last_scan, last_state_change
            FROM shares
            WHERE protocol = %(p)s
                AND hostname = %(h)s
                AND port = %(port)s
            """, {'p': proto, 'h': hostname, 'port': port})
        try:
            share_id, tree_id, hostaddr, state, scantime, changetime = cursor.fetchone()
            url['share'] = [('s', share_id)]
        except:
            return render_to_response('vfs/error.html',
                {'error':"Unknown share."})
    if scantime == None:
        return render_to_response('vfs/error.html',
            {'error':"Sorry, this share hasn't been scanned yet."})
    # detect path
    if goup > 0:
        try:
            cursor.execute("SELECT * FROM path_goup(%(t)s, %(p)s, %(l)s)",
                           {'t': tree_id, 'p': path_id, 'l': goup})
            path_id, page_offset = cursor.fetchone()
            page_offset = page_offset / vfs_items_per_page
            # update get to open the correct page  
            request.GET = request.GET.copy()
            request.GET.update({'o': page_offset})
        except:
            return render_to_response('vfs/error.html',
                {'error':"Wrong parameters."})
    url['path'] = [('p', path_id)]
    url['offset'] = [('o', page_offset)] if page_offset > 0 else []
    if path_id != 0:
        redirect_url = "./?" + urlencode(dict(url['share'] + url['offset']))
        cursor.execute("""
            SELECT path, parent_id, parentfile_id, items, size
            FROM paths
            WHERE tree_id = %(t)s AND treepath_id = %(p)s
            """, {'t':tree_id, 'p':path_id})
        try:
            dbpath, parent_id, parentfile_id, items, size = cursor.fetchone()
            if path != unicode(dbpath, "utf-8"):
                return HttpResponseRedirect(redirect_url)
        except:
            return HttpResponseRedirect(redirect_url)
    else:
        cursor.execute("""
            SELECT treepath_id, parent_id, parentfile_id, items, size
            FROM paths
            WHERE tree_id = %(t)s AND path = %(p)s
            """, {'t': tree_id, 'p': path})
        try:
            path_id, parent_id, parentfile_id, items, size = cursor.fetchone()
            url['path'] = [('p', path_id)]
        except:
            return render_to_response('vfs/error.html',
                {'error':"No such file or directory: '" + path + "'"})
    try:
        path_id = int(path_id)
        parent_id = int(parent_id)
        parentfile_id = int(parentfile_id)
        items = int(items)
        size = int(size)
    except:
        return render_to_response('vfs/error.html',
            {'error':"Seems like directory '%s' has not been scanned properly." % path})
    # detect offset in file list and fill offset bar
    offset, gobar = offset_prepare(request, items, vfs_items_per_page)
    # get file list
    orders = {
        'size': """
            SELECT treedir_id AS dirid, created, size, name
            FROM files
            WHERE tree_id = %(t)s
                AND treepath_id = %(p)s
            ORDER BY size DESC
            OFFSET %(o)s
            LIMIT %(l)s;
            """,
        'name': """
            SELECT treedir_id AS dirid, created, size, name
            FROM files
            WHERE tree_id = %(t)s
                AND treepath_id = %(p)s
                AND pathfile_id >= %(o)s
            ORDER BY pathfile_id
            LIMIT %(l)s;
            """
    }
    if not orders.get(order):
        order = 'name'
    cursor.execute(orders[order], {'t': tree_id, 'p': path_id, 'o': offset,
                                   'l':vfs_items_per_page})
    # some additional variables for template
    hostaddr = hostname_prepare(request, proto, hostname, hostaddr)
    if port != "0":
        hostname += ":" + port
        hostaddr += ":" + port
    if path != "":
        path = "/" + path
    urlproto = protocol_prepare(request, proto)
    if parent_id != 0:
        uplink_offset = int(parentfile_id) / vfs_items_per_page
        fastuplink = "?" + urlencode(dict(
            url['share'] + [('p', parent_id)] +
            ([('o', uplink_offset)] if uplink_offset > 0 else [])))
    else:
        fastuplink = ""
    fastselflink = "./?" + urlencode(dict(url['share'] + url['path'] + url['order']))
    orderbar = dict()
    orderbar['nontrivial'] = (cursor.rowcount > 1)
    orderbar['order'] = "./?" + urlencode(dict(url['share'] + url['path']))
    orderbar['orders'] = [{'n': k, 's': k == order} for k in orders.keys()]
    return render_to_response('vfs/share.html', \
        {'files': cursor.fetchall(),
         'protocol': proto,
         'urlproto': urlproto,
         'urlhost': hostname,
         'urladdr': hostaddr,
         'urlpath': path,
         'items': items,
         'size': size,
         'share_id': share_id,
         'fastup': fastuplink,
         'fastself': fastselflink,
         'offset': offset,
         'gobar': gobar,
         'orderbar': orderbar,
         'state': state,
         'changetime': changetime,
         'scantime': scantime,
         'gentime': time.time() - generation_started,
         'now': datetime.datetime.now(),
         })

