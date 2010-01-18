# webuguu.vfs.views - vfs view for django framework
#
# Copyright 2010, savrus
# Read the COPYING file in the root of the source tree.
#

import psycopg2
from django.http import HttpResponse
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
                h=db_host, u=db_user, p=db_password, d=db_database))
    except:
        return HttpResponse("Unable to connect to the database.")
    return render_to_response('vfs/index.html')

def net(request):
    try:
        db = psycopg2.connect(
            "host='{h}' user='{u}' " \
            "password='{p}' dbname='{d}'".format(
                h=db_host, u=db_user, p=db_password, d=db_database))
    except:
        return HttpResponse("Unable to connect to the database.")
    cursor = db.cursor()
    cursor.execute("SELECT network_id, name FROM network")
    return render_to_response('vfs/net.html', \
        {'networks': cursor.fetchall()})

def network(request, network):
    try:
        db = psycopg2.connect(
            "host='{h}' user='{u}' " \
            "password='{p}' dbname='{d}'".format(
                h=db_host, u=db_user, p=db_password, d=db_database))
    except:
        return HttpResponse("Unable to connect to the database.")
    cursor = db.cursor()
    cursor.execute("""
        SELECT share.share_id, host.name, size, proto.name
        FROM host
        LEFT JOIN share ON (host.host_id = share.host_id)
        LEFT JOIN sharetype ON (share.sharetype_id = sharetype.sharetype_id)
        LEFT JOIN proto ON (sharetype.proto_id = proto.proto_id)
        LEFT JOIN file ON (share.share_id = file.share_id
                           AND file.parent_within_share_id = 0)
        WHERE host.network_id =
            (SELECT network_id FROM network WHERE name = %(n)s)
        ORDER BY host.host_id
        """, {'n':network})
    return render_to_response('vfs/network.html', \
        {'shares': cursor.fetchall()})

def share(request, proto, hostname, path=""):
    try:
        db = psycopg2.connect(
            "host='{h}' user='{u}' " \
            "password='{p}' dbname='{d}'".format(
                h=db_host, u=db_user, p=db_password, d=db_database))
    except:
        return HttpResponse("Unable to connect to the database.")
    cursor = db.cursor()
    #TODO split hostname into hostname and port
    cursor.execute("""
        SELECT share_id
        FROM share
        WHERE sharetype_id =
                (SELECT sharetype_id
                FROM sharetype
                LEFT JOIN proto ON (sharetype.proto_id = proto.proto_id)
                WHERE proto.name = %(p)s)
            AND host_id =
            (SELECT host_id FROM host WHERE name = %(h)s)
        """, {'p': proto, 'h': hostname})
    try:
        share_id, = cursor.fetchone()
    except:
        return HttpResponse("Unknown share");
    cursor.execute("""
        SELECT path_within_share_id, parent_id
        FROM path
        WHERE share_id = %(s)s AND path = %(p)s
        """, {'s': share_id, 'p': path})
    try:
        path_id, parent_id = cursor.fetchone()
    except:
        return HttpResponse("No such file or directory '" + path + "'")
    #cursor.execute("""
    #    SELECT path
    #    FROM path
    #    WHERE share_id = %(s)s AND path_within_share_id = %(p)s
    #    """, {'s':share_id, 'p':parent_id})
    #try:
    #    path, = cursor.fetchone()
    #    out = "Up: " + path + "<br><br>"
    #except:
    #    out = ""
    if parent_id == 0:
        cursor.execute("""
            SELECT network.name
            FROM share
            LEFT JOIN host ON (share.host_id = host.host_id)
            LEFT JOIN network ON (host.network_id = network.network_id)
            WHERE share_id = %(s)s
        """, {'s': share_id})
        network, = cursor.fetchone()
    else:
        network = ""
    cursor.execute("""
        SELECT path_within_share_id, size, name
        FROM file
        LEFT JOIN filename ON (file.filename_id = filename.filename_id)
        WHERE share_id = %(s)s
            AND parent_within_share_id = %(p)s
        ORDER BY file_within_path_id;
        """, {'s': share_id, 'p': path_id})
    return render_to_response('vfs/share.html', \
        {'files': cursor.fetchall(), 'network': network})

