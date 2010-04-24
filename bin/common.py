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

# Logging options
# required by all scripts
#whether to send logs from low-level scanners to stderr
scanners_logging = True
#scripts logging routine
import time
import sys
def log(logstr, logparams = ()):
   sys.stderr.write("[" + time.ctime() + "] " + (logstr % logparams) + "\n")
   sys.stderr.flush()

def sharestr(proto, host, port=0):
    return "%s://%s%s" % (proto, host, ":" + str(port) if port != 0 else "")

#locale for scanners output
scanners_locale = "utf-8"
#path where scanners are, with trailing slash
import subprocess
import os.path
scanners_path = os.path.dirname(os.path.abspath(sys.argv[0]))
def run_scanner(cmd, ip, proto, port, ext = ""):
    """ executes scanner, returns subprocess.Popen object """
    cmd = os.path.join(scanners_path, cmd)
    if port == 0:
        cmdline = "%s %s %s" % (cmd, ext, ip)
    else:
        cmdline = "%s -p%s %s %s" % (cmd, str(port), ext, ip)
    _stderr = None
    if not scanners_logging:
        _stderr = subprocess.PIPE
    process = subprocess.Popen(cmdline, shell=True, stdin=subprocess.PIPE,
                               stdout=subprocess.PIPE, stderr=_stderr,
                               universal_newlines=True)
    process.stdin.close()
    return process


# Fill types, protocols before calling dbinit.py 
# copy known_filetypes and knows_protocols to webuguu/common.py
# required by dbinit.py
known_protocols = ('smb', 'ftp')
known_filetypes = ('dir', 'video', 'audio', 'archive', 'cdimage', 'exe', 'lib',
                   'script', 'image', 'document')
#types of files recognizable by the spider
filetypes = {
    'audio':        ('mp3', 'ogg', 'vaw', 'flac', 'ape', 'au', 'aiff',
                     'ra', 'wma', 'mid', 'midi', 'mpc'),
    'video':        ('mkv', 'avi', 'mp4', 'mov', 'mpeg', 'mpg', 'asf',
                     'ogm', 'vob', 'm2v', 'ts', 'wmv', 'rm', 'mov',
                     'qt', 'mpe', 'divx', 'flv', 'm2ts'),
    'archive':      ('bz', 'gz', 'bz2', 'tar', 'tbz', 'tgz', 'zip',
                     'rar', 'arj', 'zip', 'rar', 'ace', 'cab', 'z',
                     'tbz2', '7z', 'ha', 'jar', 'lzh', 'lzma', 'j',
                     'deb', 'rpm'),
    'cdimage':      ('bin', 'iso', 'cue', 'bwt', 'ccd', 'cdi', 'mds',
                     'nrg', 'vcd', 'vc4', 'mdf', 'gho', 'ghs'),
    'exe':          ('exe', 'com', 'msi', 'scf', 'swf'),
    'lib':          ('dll', 'sys', 'vxd', 'bpl', 'cpl', 'ime', 'drv',
                     'mui', 'so', 'lib'),
    'script':       ('bat', 'cmd', 'vbs', 'vbe', 'js', 'jse', 'wsf',
                     'wsh', 'pl', 'py', 'php', 'sh'),
    'image':        ('jpg', 'jpeg', 'gif', 'png', 'bmp', 'tiff', 'pbm',
                     'pcx', 'pnm', 'ico', 'ppm', 'psd', 'tif', 'tga',
                     'xbm', 'xpm'),
    'document':     ('txt', 'doc', 'xls', 'rtf', 'docx', 'xlsx',
                     'pdf', 'ps', 'eps', 'hlp', 'djvu', 'djv', 'htm',
                     'html', 'chm', 'mht', 'shtml', 'log', 'xml',
                     'csv', 'ini', 'cfg', 'conf', 'tex', 'dvi'),
}


#default protocols' ports, keep in conformance with known_protocols
#required by pinger.py, lookup.py
default_ports = dict(zip(known_protocols, (445, 21,)))

# Time period to wait until next scan
# required by spider.py
wait_until_next_scan = "12 hour"
wait_until_next_scan_failed = "2 hour"

# Time periods required by lookup.py:
#time period to wait until the next lookup test after successful lookup
wait_until_next_lookup = "1 week"
#time period to wait until delete old share
wait_until_delete_share = "4 month"


# maximum number of lines to get from scanner
# required by spider.py
max_lines_from_scanner = 1000000


