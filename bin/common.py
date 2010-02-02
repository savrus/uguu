#
# common.py - common functions and setting for all uguu server scripts
#
# Copyright (c) 2010, Radist <radist.nt@gmail.com>
# Read the COPYING file in the root of the source tree.
#

#database connection options
#required by all scripts
import psycopg2
from psycopg2.extras import DictConnection
db_host = "localhost"
db_user = "postgres"
db_password = ""
db_database = "uguu"
def connectdb():
    return psycopg2.connect(
    	host = db_host,
    	user = db_user,
    	password = db_password,
    	database = db_database,
    	connection_factory = DictConnection)


#locale for scanners output
scanners_locale = "utf-8"
#path where scanners are
scanners_path="/home/savrus/devel/uguu/scanners"
#types of files recognizable by the spider
filetypes = dict(
    [(x,'audio') for x in ('mp3', 'ogg', 'vaw', 'flac', 'ape')] +
    [(x,'video') for x in ('mkv', 'avi', 'mp4', 'mov')] +
    [(x,'document') for x in ('txt', 'doc', 'xls', 'rtf')] +
    [(x,'archive') for x in ('bz', 'gz', 'bz2', 'tar', 'tbz', 'tgz', 'zip', 'rar', 'arj')] +
    [(x,'image') for x in ('jpg', 'jpeg', 'gif', 'png', 'bmp', 'tiff')]
)


#default protocols' ports
#required by pinger
default_ports = {'ftp': 21, 'smb': 139}

#nmap online checking command
nmap_cmd = "nmap -n -sP -PT%(p)s -iL -"
#online entry regexp for nmap output
#compatible with Nmap versions 4.62 (Debian Lenny) and 5.00 (WinNT 5.1)
nmap_online = "^Host (.+?) (?:appears to be|is) up"

