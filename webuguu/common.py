# webuguu.common - common functions and constans for all webuguu applications
#
# Copyright 2010, savrus
# Read the COPYING file in the root of the source tree.
#

import psycopg2
from psycopg2.extras import DictConnection

db_host = "localhost"
db_user = "postgres"
db_password = ""
db_database = "uguu"

# number of files in file list, shares in share list, etc
vfs_items_per_page = 10

def connectdb():
    return psycopg2.connect(
        "host='{h}' user='{u}' " \
        "password='{p}' dbname='{d}'".format(
            h=db_host, u=db_user, p=db_password, d=db_database),
        connection_factory=DictConnection)


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

