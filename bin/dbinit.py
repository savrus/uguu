#!/usr/bin/python
#
# dbinit.py - initialise uguu database
#
# Copyright 2010, savrus
# Read the COPYING file in the root of the source tree.
#

import psycopg2
import sys
from common import connectdb, known_filetypes, known_protocols

def drop(db):
    cursor = db.cursor()
    cursor.execute("""
        DROP TABLE IF EXISTS networks, scantypes, shares, paths,
            filenames, files CASCADE;
        DROP FUNCTION IF EXISTS share_state_change() CASCADE;
        DROP TYPE IF EXISTS filetype, proto CASCADE;
        DROP TEXT SEARCH CONFIGURATION IF EXISTS uguu CASCADE;
        DROP LANGUAGE IF EXISTS 'plpgsql' CASCADE;
        """)


def ddl(db):
    cursor = db.cursor()
    cursor.execute("""
        CREATE TYPE filetype AS ENUM %(filetypes)s;
        CREATE TYPE proto AS ENUM %(protocols)s;
        CREATE TABLE networks (
            network varchar(32) PRIMARY KEY
        );
        CREATE TABLE scantypes (
            scantype_id SERIAL PRIMARY KEY,
            scan_command text
        );
        CREATE TABLE shares (
            share_id SERIAL PRIMARY KEY,
            scantype_id integer REFERENCES scantypes ON DELETE RESTRICT,
            network varchar(32) REFERENCES networks ON DELETE CASCADE,
            protocol proto NOT NULL,
            hostname varchar(64) NOT NULL,
            port smallint DEFAULT 0,
            state boolean DEFAULT FALSE,
            size bigint DEFAULT 0,
            last_state_change timestamp,
            last_scan timestamp,
            UNIQUE (protocol, hostname, port)
        );
        CREATE TABLE paths (
            share_id integer REFERENCES shares ON DELETE CASCADE,
            sharepath_id integer,
            parent_id integer,
            parentfile_id integer,
            path text NOT NULL,
            items integer DEFAULT 0,
            size bigint DEFAULT 0,
            tspath tsvector,
            PRIMARY KEY (share_id, sharepath_id)
        );
        CREATE TABLE filenames (
            filename_id BIGSERIAL PRIMARY KEY,
            name text NOT NULL,
            type filetype,
            tsname tsvector
        );
        CREATE TABLE files (
            share_id integer,
            sharepath_id integer,
            pathfile_id integer,
            sharedir_id integer DEFAULT 0,
            size bigint DEFAULT 0,
            filename_id bigint REFERENCES filenames ON DELETE RESTRICT,
            FOREIGN KEY (share_id, sharepath_id) REFERENCES paths
                ON DELETE CASCADE,
            PRIMARY KEY (share_id, sharepath_id, pathfile_id)
        );
        """, {'filetypes': known_filetypes,
              'protocols': known_protocols})

# Warning: you may need to execute
# "CREATE LANGUAGE 'plpgsql';" before calling this
def ddl_prog(db):
    cursor = db.cursor()
    cursor.execute("""
        CREATE LANGUAGE 'plpgsql'; 
        CREATE OR REPLACE FUNCTION share_state_change()
            RETURNS trigger AS
            $$BEGIN
                IF NEW.state != OLD.state THEN
                    NEW.last_state_change = 'now';
                END IF;
                RETURN NEW;
            END;$$
            LANGUAGE 'plpgsql' VOLATILE COST 100;
        CREATE TRIGGER share_stage_change_trigger
            BEFORE UPDATE ON shares FOR EACH ROW
            EXECUTE PROCEDURE share_state_change();
	""")


def fill(db):
    cursor = db.cursor()
    cursor.execute("""
        INSERT INTO networks (network)
        VALUES ('localnet');

        INSERT INTO scantypes (scan_command)
        VALUES ('smbscan/smbscan');

        INSERT INTO shares (scantype_id, network, protocol, hostname)
        VALUES (1, 'localnet', 'smb', '127.0.0.1'),
               (1, 'localnet', 'smb', 'localhost');
        """)


def textsearch(db):
    cursor = db.cursor()
    cursor.execute("""
        CREATE TEXT SEARCH CONFIGURATION uguu
        (COPY = pg_catalog.english);

        ALTER TEXT SEARCH CONFIGURATION uguu
        ALTER MAPPING for asciiword, numword WITH english_stem;
        
        ALTER TEXT SEARCH CONFIGURATION uguu
        ALTER MAPPING for word WITH russian_stem;
        """)


try:
    db = connectdb()
except:
    print "I am unable to connect to the database, exiting."
    sys.exit()

drop(db)
ddl(db)
ddl_prog(db)
fill(db)
textsearch(db)
db.commit()

