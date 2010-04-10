#
# dbinit.py - initialise uguu database
#
# Copyright 2010, savrus
# Copyright (c) 2010, Radist <radist.nt@gmail.com>
# Read the COPYING file in the root of the source tree.
#

import psycopg2
import sys
import common
import getpass
from common import connectdb, known_filetypes, known_protocols

def db_is_empty(db):
    cursor = db.cursor()
    def check_q(query, required_names):
        cursor.execute(query)
        for line in cursor.fetchall():
            if line[0] in required_names:
                return False
        return True    
    def check(type_name, required_names):
        return check_q("""
            SELECT %(t)s_name
            FROM information_schema.%(t)ss
            WHERE %(t)s_schema='public'
            """ % {'t': type_name}, required_names)
    return check('table', ['networks', 'scantypes', 'shares', 'paths', \
                           'files', 'trees']) and \
           check('routine', ['share_state_change']) and \
           check('trigger', ['share_stage_change_trigger']) and \
           check('sequence', ['scantypes_scantype_id_seq', 'shares_id_seq', \
                              'trees_tree_id_seq']) and \
           check_q("""
            SELECT typname FROM pg_type
            JOIN pg_namespace ON pg_type.typnamespace=pg_namespace.oid
            WHERE pg_namespace.nspname='public' AND
                pg_type.oid IN (SELECT DISTINCT(enumtypid) FROM pg_enum)
            """, ['availability', 'filetype', 'proto'])

def safe_query(db, query):
    db.commit()
    try:
        db.cursor().execute(query)
    except:
        db.rollback()

        CREATE INDEX paths_path ON paths USING hash(path);
        CREATE INDEX filenames_name ON files USING hash(name);
        CREATE INDEX files_name ON files USING hash(lower(name));
        CREATE INDEX files_tsname ON files USING gin(tsname);
        CREATE INDEX files_type ON files (type);
        CREATE INDEX files_size ON files (size);
        CREATE INDEX files_tsfullpath ON files USING gin((tspath || tsname));
        CREATE INDEX shares_hostname ON shares USING hash(hostname);
        CREATE INDEX shares_network ON shares USING hash(network);
        CREATE INDEX shares_state ON shares USING hash(state);
        CREATE INDEX shares_tree_id ON shares (tree_id);
        CREATE INDEX trees_hash ON trees USING hash(hash);
def drop(db):
    cursor = db.cursor()
    cursor.execute("""
        DROP INDEX IF EXISTS filenames_name, files_type,
            paths_path, files_name, files_size, files_tsname,
            files_tsfullpath, trees_hash, shares_tree_id,
            shares_hostname, shares_network, shares_state;
        DROP TABLE IF EXISTS networks, scantypes, trees, shares,
            paths, files CASCADE;
        """)
    safe_query(db, "DROP FUNCTION IF EXISTS share_state_change() CASCADE")
    cursor.execute("""
        DROP TYPE IF EXISTS filetype, proto, availability CASCADE;
        DROP TEXT SEARCH CONFIGURATION IF EXISTS uguu CASCADE;
        """)
    safe_query(db, "DROP LANGUAGE IF EXISTS 'plpgsql' CASCADE")

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
        CREATE TABLE trees (
            tree_id SERIAL PRIMARY KEY,
            hash varchar(64) UNIQUE,
            size bigint NOT NULL DEFAULT 0
        );
        ALTER SEQUENCE trees_tree_id_seq
            MINVALUE -2147483648 MAXVALUE 2147483647
            CYCLE;
        CREATE TABLE shares (
            share_id SERIAL PRIMARY KEY,
            tree_id int REFERENCES trees ON DELETE RESTRICT DEFAULT NULL,
            scantype_id integer REFERENCES scantypes ON DELETE RESTRICT NOT NULL,
            network varchar(32) REFERENCES networks ON DELETE CASCADE NOT NULL,
            protocol proto NOT NULL,
            hostname varchar(64) NOT NULL,
            hostaddr inet,
            port smallint NOT NULL DEFAULT 0,
            state availability DEFAULT 'offline',
            size bigint NOT NULL DEFAULT 0,
            last_state_change timestamp DEFAULT now(),
            last_scan timestamp,
            next_scan timestamp,
            last_lookup timestamp DEFAULT now(),
            UNIQUE (protocol, hostname, port)
        );
        CREATE TABLE paths (
            tree_id integer REFERENCES trees ON DELETE CASCADE,
            treepath_id integer,
            parent_id integer,
            parentfile_id integer,
            path text NOT NULL,
            items integer NOT NULL DEFAULT 0,
            size bigint NOT NULL DEFAULT 0,
            UNIQUE (tree_id, path),
            PRIMARY KEY (tree_id, treepath_id)
        );
        CREATE TABLE files (
            tree_id integer,
            treepath_id integer,
            pathfile_id integer,
            treedir_id integer NOT NULL DEFAULT 0,
            size bigint NOT NULL DEFAULT 0,
            name text NOT NULL,
            type filetype,
            tsname tsvector,
            tspath tsvector,
            FOREIGN KEY (tree_id, treepath_id) REFERENCES paths
                ON DELETE CASCADE,
            PRIMARY KEY (tree_id, treepath_id, pathfile_id)
        );
        """)


# Warning: you may need to execute
# "CREATE LANGUAGE 'plpgsql';" before calling this
def ddl_prog(db):
    safe_query(db, "CREATE LANGUAGE 'plpgsql'")
    cursor = db.cursor()
    cursor.execute("""
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


def ddl_index(db):
    cursor = db.cursor()
    cursor.execute("""
        CREATE INDEX paths_path ON paths USING hash(path);
        CREATE INDEX files_name ON files USING hash(lower(name));
        CREATE INDEX files_tsname ON files USING gin(tsname);
        CREATE INDEX files_type ON files (type);
        CREATE INDEX files_size ON files (size);
        CREATE INDEX files_tsfullpath ON files USING gin((tspath || tsname));
        CREATE INDEX shares_hostname ON shares USING hash(hostname);
        CREATE INDEX shares_network ON shares USING hash(network);
        CREATE INDEX shares_state ON shares USING hash(state);
        CREATE INDEX shares_tree_id ON shares (tree_id);
        CREATE INDEX trees_hash ON trees USING hash(hash);
        """)


def fill(db):
    cursor = db.cursor()
    #scantypes with greater priority will be tested before those with smaller one
    #scantypes with priority<=0 are only for manual use
    cursor.execute("""
        INSERT INTO networks (network, lookup_config)
        VALUES ('official', %(msu)s);

        INSERT INTO scantypes (protocol, scan_command, priority)
        VALUES ('smb', 'smbscan -d', 2),
               ('smb', 'smbscan -a', 1),
               ('smb', 'smbscan', 0),
               ('ftp', 'ftpscan -c cp1251 -Ra', 10),
               ('ftp', 'ftpscan -c cp1251 -Rp', 0),
               ('ftp', 'ftpscan -c cp1251',     5),
               ('ftp', 'ftpscan -c cp1251 -Ma', 0),
               ('ftp', 'ftpscan -c cp1251 -Mp', 1);
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

def grant_access(db, db_user, ReadOnly):
    cursor = db.cursor()
    cursor.execute("GRANT CONNECT, TEMP ON DATABASE %(d)s TO %(u)s" %
                   {'d': common.db_database, 'u': db_user})
    cursor.execute("GRANT SELECT ON TABLE networks, scantypes TO %(u)s" %
                   {'u': db_user})
    if ReadOnly:
        cursor.execute("""
            GRANT SELECT
            ON TABLE shares, paths, files
            TO %(u)s
            """ % {'u': db_user})
    else:
        cursor.execute("""
            GRANT SELECT, INSERT, UPDATE, DELETE
            ON TABLE shares, trees, paths, files
            TO %(u)s;
            GRANT USAGE
            ON SEQUENCE shares_share_id_seq, trees_tree_id_seq
            TO %(u)s;
            GRANT EXECUTE
            ON FUNCTION share_state_change()
            TO %(u)s;
            """ % {'u': db_user})


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print "Usage: python %s parameter(s) dbusername" % sys.argv[0]
        print "Parameters are (at least one should be specified):"
        print "  --dropdb\tdrop all uguu-related stuff from database"
        print "  --makedb\tinit uguu database"
        print "  --grant\tgrant access for R/O and R/W roles, use only with --makedb"
        print "  --\tmust be specified before dbusername starting with hyphen"
        sys.exit()

    rw_user = common.db_user
    common.db_user = sys.argv.pop()
    if common.db_user[0] == '-' and sys.argv.pop() != '--':
        print "Invalid parameters, run with no parameters for help."
        sys.exit()
    common.db_password = getpass.getpass("%s's password: " % common.db_user)
    try:
        db = connectdb()
    except:
        print "I am unable to connect to the database, exiting."
        sys.exit()

    if '--dropdb' in sys.argv:
        drop(db)
        db.commit()
    elif '--makedb' not in sys.argv:
        print "Invalid parameters, run with no parameters for help."
        sys.exit()
    elif not db_is_empty(db):
        print "Database is not empty, exiting."
        sys.exit()

    if '--makedb' in sys.argv:
        ddl_types(db)
        ddl(db)
        ddl_prog(db)
        ddl_index(db)
        fill(db)
        #fillshares_localhost(db)
        #fillshares_melchior(db)
        textsearch(db)
        db.commit()
        if '--grant' in sys.argv:
            user = raw_input("R/W user name ('--' to skip) [%s]: " % rw_user)
            if user != "":
                rw_user = user
            user = raw_input("R/O user name ('--' to skip) [%s]: " % rw_user)
            if rw_user != '--':
                grant_access(db, rw_user, False)
            if user != "" and user != '--' and user != rw_user:
                grant_access(db, user, True)
            db.commit()

