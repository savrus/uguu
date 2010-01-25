# webuguu.vfs.views - vfs view for django framework
#
# Copyright 2010, savrus
# Read the COPYING file in the root of the source tree.
#

import psycopg2
from psycopg2.extras import DictConnection
from django.http import HttpResponse
from django.http import HttpResponseRedirect
from django.http import QueryDict
from django.shortcuts import render_to_response
import string

# number of files in file list, shares in share list, etc
vfs_items_per_page = 10

db_host = "localhost"
db_user = "postgres"
db_password = ""
db_database = "uguu"

def connectdb():
    return psycopg2.connect(
        "host='{h}' user='{u}' " \
        "password='{p}' dbname='{d}'".format(
            h=db_host, u=db_user, p=db_password, d=db_database),
        connection_factory=DictConnection)

def generate_go_bar(items, offset):
    if items > 0:
        items = items - 1
    go = dict()
    go['first'] = 0
    go['last'] = items / vfs_items_per_page
    if go['last'] == 0:
        go['nontrivial'] = 0
        return go
    else:
        go['nontrivial'] = 1
    left = max(go['first'], offset - 4)
    right = min(go['last'], offset + 4)
    left_adj = offset + 4 - right
    right_adj = left - (offset - 4)
    left = max(go['first'], left - left_adj)
    right = min(go['last'], right + right_adj)
    if offset != left:
        go['prev'] = str(offset - 1)
    if offset != right:
        go['next'] = str(offset + 1)
    go['before'] = range(left, offset)
    go['self'] = offset
    go['after'] = range(offset + 1, right + 1)
    return go

def index(request):
    try:
        db = connectdb()
    except:
        return HttpResponse("Unable to connect to the database.")
    return render_to_response('vfs/index.html')


def net(request):
    try:
        db = connectdb()
    except:
        return HttpResponse("Unable to connect to the database.")
    cursor = db.cursor()
    cursor.execute("SELECT network FROM networks")
    return render_to_response('vfs/net.html', \
        {'networks': cursor.fetchall()})


def network(request, network):
    try:
        db = connectdb()
    except:
        return HttpResponse("Unable to connect to the database.")
    cursor = db.cursor()
    cursor.execute("""
        SELECT share_id, size, protocol, hostname, port
        FROM shares
        WHERE network = %(n)s
        ORDER BY hostname
        """, {'n':network})
    return render_to_response('vfs/network.html', \
        {'shares': cursor.fetchall(),
         'network': network})


def host(request, proto, hostname):
    try:
        db = connectdb()
    except:
        return HttpResponse("Unable to connect to the database.")
    cursor = db.cursor()
    cursor.execute("""
        SELECT share_id, size, network, protocol, hostname, port
        FROM shares
        WHERE hostname = %(n)s
        ORDER BY share_id
        """, {'n':hostname})
    return render_to_response('vfs/host.html', \
        {'shares': cursor.fetchall(),
         'hostname': hostname})


def share(request, proto, hostname, port, path=""):
    try:
        db = connectdb()
    except:
        return HttpResponse("Unable to connect to the database.")
    cursor = db.cursor()
    # detect share
    try:
        share_id = int(request.GET.get('s', 0))
    except:
        return HttpResponse("Wrong GET paremeters.")
    if share_id != 0:
        cursor.execute("""
            SELECT protocol, hostname, port,
                   state, last_scan, last_state_change
            FROM shares
            WHERE share_id = %(s)s
            """, {'s':share_id})
        try:
            d_proto, d_hostname, d_port, state, scantime, changetime = cursor.fetchone()
            if [proto, hostname, int(port)] != [d_proto, d_hostname, d_port]:
                return HttpResponseRedirect(".")
        except: 
            return HttpResponseRedirect(".")
    else:
        cursor.execute("""
            SELECT share_id, state, last_scan, last_state_change
            FROM shares
            WHERE protocol = %(p)s
                AND hostname = %(h)s
                AND port = %(port)s
            """, {'p': proto, 'h': hostname, 'port': port})
        try:
            share_id, state, scantime, changetime = cursor.fetchone()
        except:
            return HttpResponse("Unknown share");
    if scantime == None:
        return HttpResponse("Sorry, this share hasn't been scanned yet.")
    # detect path
    try:
        path_id = int(request.GET.get('p', 0))
    except:
        return HttpResponse("Wrong GET paremeters.")
    if path_id != 0:
        cursor.execute("""
            SELECT path, parent_id, parentfile_id, items, size
            FROM paths
            WHERE share_id = %(s)s AND sharepath_id = %(p)s
            """, {'s':share_id, 'p':path_id})
        try:
            dbpath, parent_id, parentfile_id, items, size = cursor.fetchone()
            if path != dbpath:
                return HttpResponseRedirect("./?s=" + str(share_id))
        except: 
            return HttpResponseRedirect("./?s=" + str(share_id))
    else:
        cursor.execute("""
            SELECT sharepath_id, parent_id, parentfile_id, items, size
            FROM paths
            WHERE share_id = %(s)s AND path = %(p)s
            """, {'s': share_id, 'p': path})
        try:
            path_id, parent_id, parentfile_id, items, size = cursor.fetchone()
        except:
            return HttpResponse("No such file or directory '" + path + "'")
    # detect parent offset in grandparent file list (for uplink)
    uplink_offset = int(parentfile_id)/vfs_items_per_page
    # detect offset in file list and fill offset bar
    try:
        page_offset = max(0, int(request.GET.get('o', 0)))
    except:
        return HttpResponse("Wrong GET paremeters.")
    offset = page_offset * vfs_items_per_page
    gobar = generate_go_bar(items, page_offset)
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
    ##change 'smb' to 'file' here
    #if proto == "smb":
    #    urlproto = "file"
    #else:
    #    urlproto = proto
    urlproto = proto
    if parent_id != 0:
        d = QueryDict("")
        d = d.copy()
        d.update({'s':share_id, 'p':parent_id, 'o': uplink_offset})
        fastuplink = "?" + d.urlencode()
    else:
        fastuplink = ""
    fastselflink = "./?s=" + str(share_id) + "&p=" + str(path_id)
    state = ['offline', 'online'][int(state)]
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
         'scantime': scantime
         })

