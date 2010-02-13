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
        DROP INDEX IF EXISTS filenames_name, filenames_tsname, filenames_type,
            paths_path, files_filename, files_sharedir, files_size,
            shares_hostname, shares_network, shares_state;
        DROP TABLE IF EXISTS networks, scantypes, shares, paths,
            filenames, files CASCADE;
        DROP FUNCTION IF EXISTS share_state_change() CASCADE;
        DROP TYPE IF EXISTS filetype, proto, availability CASCADE;
        DROP TEXT SEARCH CONFIGURATION IF EXISTS uguu CASCADE;
        DROP LANGUAGE IF EXISTS 'plpgsql' CASCADE;
        """)


def ddl_types(db):
    cursor = db.cursor()
    cursor.execute("""
        CREATE TYPE filetype AS ENUM %(filetypes)s;
        CREATE TYPE proto AS ENUM %(protocols)s;
        CREATE TYPE availability AS ENUM ('online', 'offline');
        """, {'filetypes': known_filetypes,
              'protocols': known_protocols})


def ddl(db):
    cursor = db.cursor()
    cursor.execute("""
        CREATE TABLE networks (
            network varchar(32) PRIMARY KEY,
            lookup_config text
        );
        CREATE TABLE scantypes (
            scantype_id SERIAL PRIMARY KEY,
            scan_command text NOT NULL,
            protocol proto NOT NULL,
            priority smallint NOT NULL DEFAULT -1
        );
        CREATE TABLE shares (
            share_id SERIAL PRIMARY KEY,
            scantype_id integer REFERENCES scantypes ON DELETE RESTRICT NOT NULL,
            network varchar(32) REFERENCES networks ON DELETE CASCADE NOT NULL,
            protocol proto NOT NULL,
            hostname varchar(64) NOT NULL,
            port smallint DEFAULT 0,
            state availability DEFAULT 'offline',
            size bigint DEFAULT 0,
            last_state_change timestamp DEFAULT now(),
            last_scan timestamp,
            next_scan timestamp,
            last_lookup timestamp DEFAULT now(),
            hash varchar(64),
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
            UNIQUE (share_id, path),
            PRIMARY KEY (share_id, sharepath_id)
        );
        CREATE TABLE filenames (
            filename_id BIGSERIAL PRIMARY KEY,
            name text UNIQUE NOT NULL,
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
            tsfullpath tsvector,
            FOREIGN KEY (share_id, sharepath_id) REFERENCES paths
                ON DELETE CASCADE,
            PRIMARY KEY (share_id, sharepath_id, pathfile_id)
        );
        """)


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
        
        CREATE OR REPLACE FUNCTION gfid(
                IN text, IN filetype, IN text)
            RETURNS integer AS $$
            DECLARE id INTEGER;
            BEGIN
                LOCK TABLE filenames IN ROW EXCLUSIVE MODE;
                SELECT INTO id filename_id FROM filenames WHERE name = $1;
                IF NOT FOUND THEN
                    INSERT INTO filenames (name, type, tsname)
                    VALUES ($1, $2, to_tsvector($3));
                    RETURN lastval();
                END IF;
                RETURN id;
            END;
            $$ LANGUAGE 'plpgsql' VOLATILE;
	""")


def ddl_index(db):
    cursor = db.cursor()
    cursor.execute("""
        CREATE INDEX filenames_name ON filenames USING hash(name);
        CREATE INDEX filenames_tsname ON filenames USING gin(tsname);
        CREATE INDEX filenames_type ON filenames (type);
        CREATE INDEX paths_path ON paths USING hash(path);
        CREATE INDEX paths_tspath ON paths USING gin(tspath);
        CREATE INDEX files_filename ON files (filename_id);
        CREATE INDEX files_sharedir ON files ((sharedir_id != 0));
        CREATE INDEX files_size ON files (size);
        CREATE INDEX shares_hostname ON shares USING hash(hostname);
        CREATE INDEX shares_network ON shares USING hash(network);
        CREATE INDEX shares_state ON shares USING hash(state);
        """)


def fill(db):
    cursor = db.cursor()
    cursor.execute("""
        INSERT INTO networks (network, lookup_config)
        VALUES ('official', %(msu)s);

        INSERT INTO scantypes (protocol, scan_command, priority)
        VALUES ('smb', 'smbscan -d', 2),
               ('smb', 'smbscan -a', 1),
               ('ftp', 'ftpscan -c cp1251', 1);
        """, {'msu': """
# retrieving computer's DNS records in official MSU network
[StandardHosts]
;melchior.msu will be the first host
list = ("melchior.msu")
[StandardHosts]
;green.msu don't have record in a.msu
list = ("green.msu")
.COMMENT
[SkipHosts]
list=("melchior.a.msu")
.END
[FlushDNSCache]
[DNSZoneToCache]
Zone = "a.msu"
DNSAddr = "ns.msu"
ValExclude="^auto-\w{10}\.a\.msu$"
Suffix = ".msu"
[DNSZoneKeys]
Zone = "a.msu"
Type = "A"
DNSAddr = "ns.msu"
ValExclude="^auto-\w{10}\.a\.msu$"
Suffix = ".msu"
        """})

def fillshares_melchior(db):
    cursor = db.cursor()
    cursor.execute("""
        INSERT INTO shares (scantype_id, network, protocol, hostname)
        VALUES (1, 'official', 'smb', 'melchior.msu'),
               (2, 'official', 'ftp', 'melchior.msu');
        """)

def fillshares_localhost(db):
    cursor = db.cursor()
    cursor.execute("""
        INSERT INTO shares (scantype_id, network, protocol, hostname)
        VALUES (1, 'official', 'smb', '127.0.0.1'),
               (1, 'official', 'smb', 'localhost');
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


if __name__ == "__main__":
    try:
        db = connectdb()
    except:
        print "I am unable to connect to the database, exiting."
        sys.exit()

    drop(db)
    ddl_types(db)
    ddl(db)
    ddl_prog(db)
    ddl_index(db)
    fill(db)
    #fillshares_localhost(db)
    textsearch(db)
    db.commit()

