# webuguu.common - common functions and constans for all webuguu applications
#
# Copyright 2010, savrus
# Read the COPYING file in the root of the source tree.
#

import re
import string
from django.core.urlresolvers import reverse

# database connection settings
import psycopg2
from psycopg2.extras import DictConnection
db_host = "localhost"
db_user = "postgres"
db_password = ""
db_database = "uguu"
db_setapp = True # should be false for PostgreSQL version prior to 9
def connectdb(app):
    conn = psycopg2.connect(
        host = db_host,
        user = db_user,
        password = db_password,
        database = db_database,
        connection_factory = DictConnection)
    if db_setapp:
        conn.cursor().execute("set application_name = %(app)s",
                              {'app': "Uguu search web" if app is None else app + " (uguu web)"})
    return conn

# this must be the same as in bin/common.py
known_protocols = ('smb', 'ftp', 'http')
known_filetypes = ('dir', 'video', 'audio', 'archive', 'cdimage', 'exe', 'lib',
                   'script', 'image', 'document')
# types selectable by user search. 'type:' value corresponds to filetypes
# table in bin/common.py with single exception for 'type:dir'
usertypes = (
    {'value':'', 'text':'All'},
    {'value':'type:video min:300Mb', 'text':'Films'},
    {'value':'type:video max:400Mb', 'text':'Clips'},
    {'value':'type:audio', 'text':'Audio'},
    {'value':'type:archive', 'text':'Archives'},
    {'value':'type:cdimage', 'text':'CD/DVD Images'},
    {'value':'type:exe,script', 'text':'Executables'},
    {'value':'type:exe,lib', 'text':'Binary'},
    {'value':'type:image', 'text':'Images'},
    {'value':'type:document', 'text':'Documents'},
    {'value':'type:dir', 'text':'Directories'}
)


# number of files in file list, shares in share list, etc
vfs_items_per_page = 50
# number of search results per page
search_items_per_page = 50


# number of items in rss feed
rss_items = 100

# add an item too rss feed
def rss_feed_add_item(request, feed, name, desc):
    feed.add_item(
        title = name,
        link = "http://%(r)s%(v)s?q=%(q)s match:exact"
            % {'r': request.META['HTTP_HOST'],
               'v': reverse('webuguu.search.views.search'),
               'q': name},
        description = desc)


# gobar generator
def generate_go_bar(items, offset):
    if items > 0:
        items = items - 1
    go = dict()
    go['first'] = 0
    go['last'] = items / vfs_items_per_page
    if go['last'] == 0:
        go['nontrivial'] = 0
        return go
    else:
        go['nontrivial'] = 1
    left = max(go['first'], offset - 4)
    right = min(go['last'], offset + 4)
    left_adj = offset + 4 - right
    right_adj = left - (offset - 4)
    left = max(go['first'], left - left_adj)
    right = min(go['last'], right + right_adj)
    if offset != left:
        go['prev'] = str(offset - 1)
    if offset != right:
        go['next'] = str(offset + 1)
    go['before'] = range(left, offset)
    go['self'] = offset
    go['after'] = range(offset + 1, right + 1)
    return go


def offset_prepare(request, items, items_per_page):
    page_offset = int(request.GET.get('o', 0))
    page_offset = max(0, min((items - 1) / items_per_page, page_offset))
    offset = page_offset * items_per_page
    gobar = generate_go_bar(items, page_offset)
    return offset, gobar


def protocol_prepare(request, protocol):
    if protocol == "smb":
        if re.search(r'(?u)win(dows|nt|32)',
                string.lower(request.META['HTTP_USER_AGENT']), re.UNICODE):
            return "file://///"
    return protocol + "://"

def hostname_prepare(request, protocol, hostname, hostaddr):
    if hostaddr == None:
        return hostname
    if protocol == "smb":
        return hostaddr
    return hostname

# Some additional debug info
def debug_virtual_host(request):
    if request.get_host() in ["127.0.0.1:8000"]:
        return True
    return False

