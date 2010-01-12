#!/usr/bin/python
#
# dbinit.py - initialise uguu database
#
# Copyright 2010, savrus
# Read the COPYING file in the root of the source tree.
#

import psycopg2
import sys

db_host = "localhost"
db_user = "postgres"
db_password = ""
db_database = "uguu"

def ddl(db):
    cursor = db.cursor()
    cursor.execute("""
        CREATE TABLE network (
            network_id SERIAL PRIMARY KEY,
            ip_range varchar(256),
            name varchar(64)
        )""")
    cursor.execute("""
        CREATE TABLE host (
            host_id SERIAL PRIMARY KEY,
            network_id integer REFERENCES network ON DELETE CASCADE,
            name varchar(64),
            ip_address inet
        )""")
    cursor.execute("""
        CREATE TABLE sharetype (
            sharetype_id SERIAL PRIMARY KEY,
            protocol smallint,
            scan_command text
        )""")
    cursor.execute("""
        CREATE TABLE share (
            share_id SERIAL PRIMARY KEY,
            sharetype_id integer REFERENCES sharetype ON DELETE RESTRICT,
            host_id integer REFERENCES host ON DELETE CASCADE
        )""")
    cursor.execute("""
        CREATE TABLE path (
            share_id integer REFERENCES share ON DELETE CASCADE,
            path_within_share_id integer,
            parent_id integer,
            path text,
            PRIMARY KEY (share_id, path_within_share_id)
        )""")
    cursor.execute("""
        CREATE TABLE filename (
            filename_id BIGSERIAL PRIMARY KEY,
            name text
        )""")
    cursor.execute("""
        CREATE TABLE file (
            share_id integer,
            parent_within_share_id integer,
            path_within_share_id integer,
            file_within_path_id integer,
            size bigint,
            filename_id bigint REFERENCES filename ON DELETE RESTRICT,
            file_type smallint,
            PRIMARY KEY (share_id, parent_within_share_id, file_within_path_id)
        )""")

def fill(db):
    cursor = db.cursor()
    cursor.execute("""
        INSERT INTO network (ip_range, name)
        VALUES ('127.0.0.1', 'localnet')
        """)
    cursor.execute("""
        INSERT INTO host (network_id, name, ip_address)
        VALUES (1, 'localhost', '127.0.0.1')
        """)
    cursor.execute("""
        INSERT INTO sharetype (protocol, scan_command)
        VALUES (0, 'smbscan')
        """)
    cursor.execute("""
        INSERT INTO share (sharetype_id, host_id)
        VALUES (1, 1)
        """)


try:
    db = psycopg2.connect(
        "host='{host}' user='{user}' " \
        "password='{password}' dbname='{dbname}'".format(
            host=db_host, user=db_user,
            password=db_password, dbname=db_database)
        )
except:
    print "I am unable to connect to the database, exiting."
    sys.exit()

ddl(db)
fill(db)
db.commit()

