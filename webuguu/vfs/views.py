# webuguu.vfs.views - vfs view for django framework
#
# Copyright 2010, savrus
# Read the COPYING file in the root of the source tree.
#

import psycopg2
from psycopg2.extras import DictConnection
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
    cursor.execute("SELECT network_name AS name FROM networks")
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
        SELECT share_id, hosts.name AS hostname, size, protocol, port
        FROM hosts
        LEFT JOIN shares ON (hosts.host_id = shares.host_id)
        WHERE hosts.network_name = %(n)s
        ORDER BY hosts.host_id
        """, {'n':network})
    return render_to_response('vfs/network.html', \
        {'shares': cursor.fetchall()})

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
        SELECT network_name
        FROM hosts
        WHERE name = %(h)s
    """, {'h': hostname})
    network, = cursor.fetchone()
    cursor.execute("""
        SELECT share_id, size, protocol, port
        FROM hosts
        LEFT JOIN shares ON (hosts.host_id = shares.host_id)
        WHERE hosts.name = %(n)s
        ORDER BY hosts.host_id
        """, {'n':hostname})
    return render_to_response('vfs/host.html', \
        {'shares': cursor.fetchall(), 'hostname': hostname,
            'network': network})


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
    cursor.execute("""
        SELECT share_id
        FROM shares
        WHERE protocol = %(p)s
            AND host_id =
            (SELECT host_id FROM hosts WHERE name = %(h)s)
            AND port = %(port)s
        """, {'p': proto, 'h': hostname, 'port': port})
    try:
        share_id, = cursor.fetchone()
    except:
        return HttpResponse("Unknown share");
    cursor.execute("""
        SELECT sharepath_id, parent_id
        FROM paths
        WHERE share_id = %(s)s AND path = %(p)s
        """, {'s': share_id, 'p': path})
    try:
        path_id, parent_id = cursor.fetchone()
    except:
        return HttpResponse("No such file or directory '" + path + "'")
    if parent_id == 0:
        cursor.execute("""
            SELECT network_name
            FROM shares
            LEFT JOIN hosts ON (shares.host_id = hosts.host_id)
            WHERE share_id = %(s)s
        """, {'s': share_id})
        network, = cursor.fetchone()
    else:
        network = ""
    cursor.execute("""
        SELECT sharedir_id AS dirid, size, name
        FROM files
        LEFT JOIN filenames ON (files.filename_id = filenames.filename_id)
        WHERE share_id = %(s)s
            AND sharepath_id = %(p)s
        ORDER BY pathfile_id;
        """, {'s': share_id, 'p': path_id})
    return render_to_response('vfs/share.html', \
        {'files': cursor.fetchall(), 'network': network})

