#!/usr/bin/python
#
# dbinit.py - initialise uguu database
#
# Copyright 2010, savrus
# Read the COPYING file in the root of the source tree.
#

import psycopg2
import sys
from common import connectdb

def drop(db):
    cursor = db.cursor()
    cursor.execute("""
        DROP TABLE IF EXISTS networks, scantypes, shares, paths,
            filenames, files CASCADE
        """)


def ddl(db):
    cursor = db.cursor()
    cursor.execute("""
        CREATE TABLE networks (
            network varchar(32) PRIMARY KEY,
            total integer DEFAULT 0
        );
        CREATE TABLE scantypes (
            scantype_id SERIAL PRIMARY KEY,
            scan_command text
        );
        CREATE TABLE shares (
            share_id SERIAL PRIMARY KEY,
            netshare_id integer,
            scantype_id integer REFERENCES scantypes ON DELETE RESTRICT,
            network varchar(32) REFERENCES networks ON DELETE CASCADE,
            protocol varchar(8),
            hostname varchar(64),
            port smallint DEFAULT 0,
            state boolean DEFAULT FALSE,
            size bigint,
            last_state_change timestamp,
            last_scan timestamp
        );
        CREATE TABLE paths (
            share_id integer REFERENCES shares ON DELETE CASCADE,
            sharepath_id integer,
            parent_id integer,
            parentfile_id integer,
            path text,
            items integer,
            size bigint,
            tspath tsvector,
            PRIMARY KEY (share_id, sharepath_id)
        );
        CREATE TABLE filenames (
            filename_id BIGSERIAL PRIMARY KEY,
            name text,
            type varchar(16),
            tsname tsvector
        );
        CREATE TABLE files (
            share_id integer,
            sharepath_id integer,
            pathfile_id integer,
            sharedir_id integer,
            size bigint,
            filename_id bigint REFERENCES filenames ON DELETE RESTRICT,
            FOREIGN KEY (share_id, sharepath_id) REFERENCES paths
                ON DELETE CASCADE,
            PRIMARY KEY (share_id, sharepath_id, pathfile_id)
        );
        """)


def ddl_prog(db):
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

        CREATE OR REPLACE FUNCTION share_handle_netshare_id()
            RETURNS trigger AS
            $$BEGIN
                IF (TG_OP = 'INSERT') THEN
                    UPDATE shares SET netshare_id = netshare_id + 1
                        WHERE shares.network = NEW.network
                            AND (shares.hostname, shares.protocol, shares.port)
                                > (NEW.hostname, NEW.protocol, NEW.port);
                    NEW.netshare_id = (SELECT max(netshare_id) + 1 FROM
                        (SELECT netshare_id FROM shares
                            WHERE shares.network = NEW.network
                            AND (shares.hostname, shares.protocol, shares.port)
                                <= (NEW.hostname, NEW.protocol, NEW.port)
                         UNION SELECT -1) AS m);
                    RETURN NEW;
                ELSIF (TG_OP = 'DELETE') THEN
                    UPDATE shares SET netshare_id = netshare_id - 1
                        WHERE shares.network = OLD.network
                            AND shares.netshare_id > OLD.netshare_id;
                    RETURN OLD;
                ELSIF (TG_OP = 'UPDATE') THEN
                    IF ( NEW.network != OLD.network
                            OR NEW.hostname != OLD.hostname
                            OR NEW.protocol != OLD.protocol
                            OR NEW.port != OLD.port) THEN
                        UPDATE shares SET netshare_id = netshare_id - 1
                            WHERE shares.network = OLD.network
                                AND shares.netshare_id > OLD.netshare_id;
                        UPDATE shares SET netshare_id = netshare_id + 1
                            WHERE shares.network = NEW.network
                            AND (shares.hostname, shares.protocol, shares.port)
                                > (NEW.hostname, NEW.protocol, NEW.port);
                        NEW.netshare_id = (SELECT max(netshare_id) + 1 FROM
                            (SELECT netshare_id FROM shares
                                WHERE shares.network = NEW.network
                                AND (shares.hostname, shares.protocol, shares.port)
                                <= (NEW.hostname, NEW.protocol, NEW.port)
                            UNION SELECT -1) AS m);
                    END IF;
                    RETURN NEW;
                END IF;
            END;$$
            LANGUAGE 'plpgsql';
        CREATE OR REPLACE FUNCTION network_handle_netshare_id()
            RETURNS trigger AS
            $$BEGIN
                IF (TG_OP = 'INSERT') THEN
                    UPDATE networks SET total = total + 1
                        WHERE networks.network = NEW.network;
                    RETURN NEW;
                ELSIF (TG_OP = 'DELETE') THEN
                    UPDATE networks SET total = total - 1
                        WHERE networks.network = NEW.network;
                    RETURN OLD;
                ELSIF (TG_OP = 'UPDATE') THEN
                    IF ( NEW.network != OLD.network ) THEN
                        UPDATE networks SET total = total - 1
                            WHERE networks.network = NEW.network;
                        UPDATE networks SET total = total + 1
                            WHERE networks.network = NEW.network;
                    END IF;
                    RETURN NEW;
                END IF;
            END;$$
            LANGUAGE 'plpgsql';
        CREATE TRIGGER share_handle_netshare_id_trigger
            BEFORE INSERT OR DELETE OR UPDATE ON shares FOR EACH ROW
            EXECUTE PROCEDURE share_handle_netshare_id();
        CREATE TRIGGER network_handle_netshare_id_trigger
            AFTER INSERT OR DELETE OR UPDATE ON shares FOR EACH ROW
            EXECUTE PROCEDURE network_handle_netshare_id();
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
        CREATE TEXT SEARCH CONFIGURATION public.uguu
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
db.commit()

