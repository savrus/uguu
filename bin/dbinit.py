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
        CREATE TABLE networks (
            network_name varchar(32) PRIMARY KEY
        );
        CREATE TABLE hosts (
            host_id SERIAL PRIMARY KEY,
            network_name varchar(32) REFERENCES networks ON DELETE CASCADE,
            name varchar(64),
            host_addr inet
        );
        CREATE TABLE scantypes (
            scantype_id SERIAL PRIMARY KEY,
            scan_command text
        );
        CREATE TABLE shares (
            share_id SERIAL PRIMARY KEY,
            scantype_id integer REFERENCES scantypes ON DELETE RESTRICT,
            host_id integer REFERENCES hosts ON DELETE CASCADE,
            protocol varchar(8),
            port smallint DEFAULT 0,
            online boolean,
            size bigint,
            last_scan timestamp
        );
        CREATE TABLE paths (
            share_id integer REFERENCES shares ON DELETE CASCADE,
            sharepath_id integer,
            parent_id integer,
            path text,
            items integer,
            PRIMARY KEY (share_id, sharepath_id)
        );
        CREATE TABLE filenames (
            filename_id BIGSERIAL PRIMARY KEY,
            name text,
            type varchar(16)
        );
        CREATE TABLE files (
            share_id integer,
            sharepath_id integer,
            pathfile_id integer,
            sharedir_id integer,
            size bigint,
            filename_id bigint REFERENCES filenames ON DELETE RESTRICT,
            PRIMARY KEY (share_id, sharepath_id, pathfile_id)
        );
        """)

def fill(db):
    cursor = db.cursor()
    cursor.execute("""
        INSERT INTO networks (network_name)
        VALUES ('localnet');

        INSERT INTO hosts (network_name, name, host_addr)
        VALUES ('localnet', 'localhost', '127.0.0.1');

        INSERT INTO scantypes (scan_command)
        VALUES ('smbscan');

        INSERT INTO shares (scantype_id, host_id, protocol)
        VALUES (1, 1, 'smb');
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

