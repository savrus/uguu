# webuguu.common - common functions and constans for all webuguu applications
#
# Copyright 2010, savrus
# Read the COPYING file in the root of the source tree.
#

# database connection settings
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



# this must be the same as in bin/common.py
known_filetypes = ( 'audio', 'video', 'document', 'archive', 'image')
# types selectable by user search. 'type:' value corresponds to filetypes
# table in bin/common.py with single exception for 'type:dir'
usertypes = (
    {'value':'',                        'text':'All'},
    {'value':'type:video min:300Mb',    'text':'Films'},
    {'value':'type:video max:400Mb',    'text':'Clips'},
    {'value':'type:audio',              'text':'Audio'},
    {'value':'type:archive',            'text':'Archives'},
    {'value':'type:dir',                'text':'Directories'}
)


# number of files in file list, shares in share list, etc
vfs_items_per_page = 10
# number of search results per page
search_items_per_page = 10


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

