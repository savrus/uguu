# webuguu.vfs.views - vfs view for django framework
#
# Copyright 2010, savrus
# Read the COPYING file in the root of the source tree.
#

from django.http import HttpResponseRedirect
from django.utils.http import urlencode
from django.shortcuts import render_to_response
from django.core.urlresolvers import reverse
import string
import time
from webuguu.common import connectdb, vfs_items_per_page, offset_prepare, protocol_prepare, known_protocols


def index(request):
    generation_started = time.time()
    try:
        db = connectdb()
    except:
        return render_to_response('vfs/error.html',
            {'error':"Unable to connect to the database."})
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
        listinfo  = cursor.fetchone()
        items = listinfo['items']
        if items == 0:
            raise
    except:
        return render_to_response('vfs/error.html',
            {'error':"No shares within %s '%s'" % (column, name)})
    offset, gobar = offset_prepare(request, items, vfs_items_per_page)
    cursor.execute("""
        SELECT share_id, state, size, network, protocol, hostname, port
        FROM shares
        WHERE %s = %%(n)s
        ORDER BY state, hostname
        OFFSET %%(o)s LIMIT %%(l)s
        """ % column, {'n':name, 'o':offset, 'l':vfs_items_per_page})
    fastselflink = "./?"
    return render_to_response("vfs/sharelist.html", \
        {'name': name,
         'ishost': is_this_host,
         'shares': cursor.fetchall(),
         'fastself': fastselflink,
         'gobar': gobar,
         'info': listinfo,
         'gentime': time.time() - generation_started,
         })


def network(request, network):
    return sharelist(request, "network", network, 0)


def host(request, proto, hostname):
    return sharelist(request, "hostname", hostname, 1)


def share(request, proto, hostname, port, path=""):
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
        url = dict()
        url['share'] = [('s', share_id)]
        url['path'] = [('p', path_id)]
        url['offset'] = [('o', page_offset)] if page_offset > 0 else []
    except:
        return render_to_response('vfs/error.html',
            {'error':"Wrong parameters."})
    # detect share
    if share_id != 0:
        cursor.execute("""
            SELECT protocol, hostname, port,
                   state, last_scan, next_scan, last_state_change
            FROM shares
            WHERE share_id = %(s)s
            """, {'s':share_id})
        try:
            d_proto, d_hostname, d_port, state, scantime, nexttime, changetime = cursor.fetchone()
            if [proto, hostname, int(port)] != [d_proto, d_hostname, d_port]:
                return HttpResponseRedirect(".")
        except: 
            return HttpResponseRedirect(".")
    else:
        cursor.execute("""
            SELECT share_id, state, last_scan, next_scan, last_state_change
            FROM shares
            WHERE protocol = %(p)s
                AND hostname = %(h)s
                AND port = %(port)s
            """, {'p': proto, 'h': hostname, 'port': port})
        try:
            share_id, state, scantime, nexttime, changetime = cursor.fetchone()
            url['share'] = [('s', share_id)] 
        except:
            return render_to_response('vfs/error.html',
                {'error':"Unknown share."})
    if scantime == None:
        return render_to_response('vfs/error.html',
            {'error':"Sorry, this share hasn't been scanned yet."})
    # detect path
    if path_id != 0:
        redirect_url = "./?" + urlencode(dict(url['share'] + url['offset']))
        cursor.execute("""
            SELECT path, parent_id, parentfile_id, items, size
            FROM paths
            WHERE share_id = %(s)s AND sharepath_id = %(p)s
            """, {'s':share_id, 'p':path_id})
        try:
            dbpath, parent_id, parentfile_id, items, size = cursor.fetchone()
            if path != unicode(dbpath, "utf-8"):
                return HttpResponseRedirect(redirect_url)
        except: 
            return HttpResponseRedirect(redirect_url)
    else:
        cursor.execute("""
            SELECT sharepath_id, parent_id, parentfile_id, items, size
            FROM paths
            WHERE share_id = %(s)s AND path = %(p)s
            """, {'s': share_id, 'p': path})
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
    cursor.execute("""
        SELECT sharedir_id AS dirid, size, name
        FROM files
        LEFT JOIN filenames ON (files.filename_id = filenames.filename_id)
        WHERE share_id = %(s)s
            AND sharepath_id = %(p)s
            AND pathfile_id >= %(o)s
        ORDER BY pathfile_id
        LIMIT %(l)s;
        """, {'s': share_id, 'p': path_id, 'o': offset, 'l':vfs_items_per_page})
    # some additional variables for template
    if port != "0":
        hostname += ":" + port
    if path != "":
        path = "/" + path
    urlproto = protocol_prepare(request, proto)
    if parent_id != 0:
        uplink_offset = int(parentfile_id) / vfs_items_per_page
        fastuplink = "?" + urlencode(dict(
            url['share'] + [('p', parent_id)] +
            ([('o', uplink_offset)] if uplink_offset > 0 else []) ))
    else:
        fastuplink = ""
    fastselflink = "./?" + urlencode(dict(url['share'] + url['path']))
    return render_to_response('vfs/share.html', \
        {'files': cursor.fetchall(),
         'protocol': proto,
         'urlproto': urlproto,
         'urlhost': hostname,
         'urlpath': path,
         'items': items,
         'size': size,
         'share_id': share_id,
         'fastup': fastuplink,
         'fastself': fastselflink,
         'offset': offset,
         'gobar': gobar,
         'state': state,
         'changetime': changetime,
         'scantime': scantime,
         'nextscantime': nexttime,
         'gentime': time.time() - generation_started,
         })

