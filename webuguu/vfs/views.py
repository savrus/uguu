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

db_host = "localhost"
db_user = "postgres"
db_password = ""
db_database = "uguu"


def index(request):
    try:
        db = psycopg2.connect(
            "host='{h}' user='{u}' " \
            "password='{p}' dbname='{d}'".format(
                h=db_host, u=db_user, p=db_password, d=db_database),
            connection_factory=DictConnection)
    except:
        return HttpResponse("Unable to connect to the database.")
    return render_to_response('vfs/index.html')


def net(request):
    try:
        db = psycopg2.connect(
            "host='{h}' user='{u}' " \
            "password='{p}' dbname='{d}'".format(
                h=db_host, u=db_user, p=db_password, d=db_database),
            connection_factory=DictConnection)
    except:
        return HttpResponse("Unable to connect to the database.")
    cursor = db.cursor()
    cursor.execute("SELECT network FROM networks")
    return render_to_response('vfs/net.html', \
        {'networks': cursor.fetchall()})


def network(request, network):
    try:
        db = psycopg2.connect(
            "host='{h}' user='{u}' " \
            "password='{p}' dbname='{d}'".format(
                h=db_host, u=db_user, p=db_password, d=db_database),
            connection_factory=DictConnection)
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
        db = psycopg2.connect(
            "host='{h}' user='{u}' " \
            "password='{p}' dbname='{d}'".format(
                h=db_host, u=db_user, p=db_password, d=db_database),
            connection_factory=DictConnection)
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
        db = psycopg2.connect(
            "host='{h}' user='{u}' " \
            "password='{p}' dbname='{d}'".format(
                h=db_host, u=db_user, p=db_password, d=db_database),
            connection_factory=DictConnection)
    except:
        return HttpResponse("Unable to connect to the database.")
    cursor = db.cursor()
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
    cursor.execute("""
        SELECT sharedir_id AS dirid, size, name
        FROM files
        LEFT JOIN filenames ON (files.filename_id = filenames.filename_id)
        WHERE share_id = %(s)s
            AND sharepath_id = %(p)s
        ORDER BY pathfile_id;
        """, {'s': share_id, 'p': path_id})
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
        d.update({'s':share_id, 'p':parent_id})
        fastuplink = "?" + d.urlencode()
    else:
        fastuplink = ""
    return render_to_response('vfs/share.html', \
        {'files': cursor.fetchall(),
         'protocol': proto,
         'urlproto': urlproto,
         'urlhost': hostname,
         'urlpath': path,
         'items':items,
         'size':size,
         'share_id':share_id,
         'fastup':fastuplink})

