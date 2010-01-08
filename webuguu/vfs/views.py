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
    cursor = db.cursor()
    cursor.execute("SELECT network_id, name FROM network")
    return render_to_response('vfs/index.html', \
        {'networks': cursor.fetchall()})

def network(request, network_id):
    try:
        db = psycopg2.connect(
            "host='{h}' user='{u}' " \
            "password='{p}' dbname='{d}'".format(
                h=db_host, u=db_user, p=db_password, d=db_database))
    except:
        return HttpResponse("Unable to connect to the database.")
    uplink = request.build_absolute_uri()
    uplink = uplink[:string.rfind(uplink,"/", 0, -1)]
    cursor = db.cursor()
    cursor.execute("""
        SELECT share.share_id, host.name, size
        FROM host
        LEFT JOIN share ON (host.host_id = share.host_id)
        LEFT JOIN file ON (share.share_id = file.share_id
                           AND file.parent_within_share_id = 0)
        WHERE host.network_id = %(n)s
        ORDER BY host.host_id
        """, {'n':network_id})
    return render_to_response('vfs/network.html', \
        {'shares': cursor.fetchall(), 'uplink': uplink})

def share(request, network_id, share_id, path=""):
    try:
        db = psycopg2.connect(
            "host='{h}' user='{u}' " \
            "password='{p}' dbname='{d}'".format(
                h=db_host, u=db_user, p=db_password, d=db_database))
    except:
        return HttpResponse("Unable to connect to the database.")
    cursor = db.cursor()
    cursor.execute("""
        SELECT path_within_share_id, parent_id
        FROM path
        WHERE share_id = %(s)s AND path = %(p)s
        """, {'s':share_id, 'p':path})
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
    out = ""
    #uplink = "http://" + request.get_host() + "/vfs/" + \
    #    str(network_id) + "/" + str(share_id) + "/" + \
    #    path[:string.rfind(path,"/")+1]
    uplink = request.build_absolute_uri()
    uplink = uplink[:string.rfind(uplink,"/", 0, -1)]
    cursor.execute("""
        SELECT path_within_share_id, size, name
        FROM file
        LEFT JOIN filename ON (file.filename_id = filename.filename_id)
        WHERE share_id = %(s)s
            AND parent_within_share_id = %(p)s
        ORDER BY file_within_path_id;
        """, {'s':share_id, 'p':path_id})
    return render_to_response('vfs/share.html', \
        {'files': cursor.fetchall(), 'uplink': uplink})

