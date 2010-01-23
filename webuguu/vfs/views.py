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
items_per_page = 10

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

def gobar(items, offset):
    go_first = 0
    go_last = items / items_per_page
    go_min = max(go_first + 1, offset - 4)
    go_max = min(go_last - 1, offset + 5)
    go_min_adj = offset + 5 - go_max
    go_max_adj = go_min - (offset - 4)
    go_min = max(go_first + 1, go_min - go_min_adj)
    go_max = min(go_last - 1, go_max + go_max_adj)
    # if you want 'first' and 'last' not to appear in numbered list try
    # go_immediate = range(go_min, go_max + 1)
    go_immediate = range(go_min - 1, go_max + 2)
    return (go_first, go_immediate, go_last)

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
    share_id = request.GET.get('s', 0)
    if share_id != 0:
        cursor.execute("""
            SELECT protocol, hostname, port
            FROM shares
            WHERE share_id = %(s)s
            """, {'s':share_id})
        try:
            if [proto, hostname, int(port)] != cursor.fetchone():
                return HttpResponseRedirect(".")
        except: 
            return HttpResponseRedirect(".")
    else:
        cursor.execute("""
            SELECT share_id
            FROM shares
            WHERE protocol = %(p)s
                AND hostname = %(h)s
                AND port = %(port)s
            """, {'p': proto, 'h': hostname, 'port': port})
        try:
            share_id, = cursor.fetchone()
        except:
            return HttpResponse("Unknown share");
    # detect path
    path_id = request.GET.get('p', 0)
    if path_id != 0:
        cursor.execute("""
            SELECT path, parent_id, items, size
            FROM paths
            WHERE share_id = %(s)s AND sharepath_id = %(p)s
            """, {'s':share_id, 'p':path_id})
        try:
            dbpath, parent_id, items, size = cursor.fetchone()
            if path != dbpath:
                return HttpResponseRedirect("./?s=" + str(share_id))
        except: 
            return HttpResponseRedirect("./?s=" + str(share_id))
    else:
        cursor.execute("""
            SELECT sharepath_id, parent_id, items, size
            FROM paths
            WHERE share_id = %(s)s AND path = %(p)s
            """, {'s': share_id, 'p': path})
        try:
            path_id, parent_id, items, size = cursor.fetchone()
        except:
            return HttpResponse("No such file or directory '" + path + "'")
    # detect offset in file list
    page_offset = int(request.GET.get('o', 0))
    offset = page_offset * items_per_page
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
        """, {'s': share_id, 'p': path_id, 'o': offset, 'l':items_per_page})
    # fill go offset bar
    go_first, go_immediate, go_last = gobar(items, page_offset)
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
    d = QueryDict("")
    d = d.copy()
    offsets_stack_list = request.GET.getlist("o")
    d.setlist('o', offsets_stack_list)
    child_offs = "&" + d.urlencode() + "&o=0"
    if parent_id != 0:
        d.update({'s':share_id, 'p':parent_id})
        d.setlist('o', offsets_stack_list[:-1])
        fastuplink = "?" + d.urlencode()
    else:
        fastuplink = ""
    fastselflink = "./?s=" + str(share_id) + "&p=" + str(path_id)
    
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
         'childoffs': child_offs,
         'offset': offset,
         'go_first': go_first,
         'go_last': go_last,
         'go_imm': go_immediate
         })

