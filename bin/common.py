#
# common.py - common functions and setting for all uguu server scripts
#
# Copyright (c) 2010, Radist <radist.nt@gmail.com>
# Read the COPYING file in the root of the source tree.
#

#
# Database connection options
# Required by: all scripts
#

import psycopg2
from psycopg2.extras import DictConnection

db_host = "localhost"
db_user = "postgres"
db_password = ""
db_database = "uguu"

#nmap online checking command
nmap_cmd = "nmap -n -sP -PT%(p)s -iL -"
#online entry regexp for nmap output
nmap_online = "^Host (.+?) appears to be up." # Nmap version 4.62 / Debian Lenny
#nmap_online = "^Host (.+?) is up" # Nmap version 5.00 / WinNT 5.1

def connectdb():
    return psycopg2.connect(
    	host = db_host,
    	user = db_user,
    	password = db_password,
    	database = db_database,
    	connection_factory = DictConnection
    )
