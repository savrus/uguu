#
# common.py - common functions and setting for all uguu server scripts
#
# Copyright (c) 2010, Radist <radist.nt@gmail.com>
# Copyright (c) 2010, savrus
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
    """ returns psycopg2 connection to the server using settings from common.py """
    return psycopg2.connect(
    	host = db_host,
    	user = db_user,
    	password = db_password,
    	database = db_database,
    	connection_factory = DictConnection)


#locale for scanners output
scanners_locale = "utf-8"
#path where scanners are, with trailing slash
import sys
from os.path import split, join
scanners_path=join(split(sys.argv[0])[0], "")


# Fill types, protocols before calling dbinit.py 
# copy known_filetypes and knows_protocols to webuguu/common.py
# required by dbinit.py
known_protocols = ('smb', 'ftp')
known_filetypes = ('audio', 'video',  'archive', 'cdimage', 'exe', 'lib',
                   'script', 'image', 'document')
#types of files recognizable by the spider
filetypes = dict(
    [(x,'audio') for x in ('mp3', 'ogg', 'vaw', 'flac', 'ape', 'au', 'aiff',
                           'ra', 'wma', 'mid', 'midi', 'mpc')] +
    [(x,'video') for x in ('mkv', 'avi', 'mp4', 'mov', 'mpeg', 'mpg', 'asf',
                           'ogm', 'vob', 'm2v', 'ts', 'wmv', 'rm', 'mov',
                           'qt', 'mpe', 'divx', 'flv', 'm2ts')] +
    [(x,'archive') for x in ('bz', 'gz', 'bz2', 'tar', 'tbz', 'tgz', 'zip',
                             'rar', 'arj', 'zip', 'rar', 'ace', 'cab', 'z',
                             'tbz2', '7z', 'ha', 'jar', 'lzh', 'lzma', 'j')] +
    [(x,'cdimage') for x in ('bin', 'iso', 'cue', 'bwt', 'ccd', 'cdi', 'mds',
                             'nrg', 'vcd', 'vc4', 'mdf', 'gho', 'ghs')] +
    [(x,'exe') for x in ('exe', 'com', 'msi', 'scf', 'swf')] +
    [(x,'lib') for x in ('dll', 'sys', 'vxd', 'bpl', 'cpl', 'ime', 'drv',
                         'mui', 'so', 'lib')] +
    [(x,'script') for x in ('bat', 'cmd', 'vbs', 'vbe', 'js', 'jse', 'wsf',
                            'wsh', 'pl', 'py', 'php', 'sh')] +

    [(x,'image') for x in ('jpg', 'jpeg', 'gif', 'png', 'bmp', 'tiff', 'pbm',
                           'pcx', 'pnm', 'ico', 'ppm', 'psd', 'tif', 'tga',
                           'xbm', 'xpm')] +
    [(x,'document') for x in ('txt', 'doc', 'xls', 'rtf', 'docx', 'xlsx',
                              'pdf', 'ps', 'eps', 'hlp', 'djvu', 'djv', 'htm',
                              'html', 'chm', 'mht', 'shtml', 'log', 'xml',
                              'csv', 'ini', 'cfg', 'conf', 'tex', 'dvi')]
)


#default protocols' ports, keep in conformance with known_protocols
#required by pinger.py, lookup.py
default_ports = dict(zip(known_protocols, (139, 21,)))

# Time period to wait until next scan
# required by spider.py
wait_until_next_scan = "12 hour"
wait_until_next_scan_failed = "2 hour"

