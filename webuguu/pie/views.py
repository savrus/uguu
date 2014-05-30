# webuguu.pie.views - pie statistics view for django framework
#
# Copyright 2012, savrus
# Read the COPYING file in the root of the source tree.
#

from django.http import HttpResponseRedirect
from django.utils.http import urlencode
from django.shortcuts import render_to_response
from webuguu.common import connectdb
import time


def pie(request):
    generation_started = time.time()
    try:
        db = connectdb()
    except:
        return render_to_response('vfs/error.html',
            {'error':"Unable to connect to the database."})
    cursor = db.cursor()
    cursor.execute("""
        SELECT hostname, MAX(size) as size FROM shares WHERE size > 0 GROUP by hostname ORDER BY size DESC
        """)
    shares = cursor.fetchall()
    cursor.execute("""
        SELECT hostname, MAX(size) as size FROM shares WHERE size > 0 and state = 'online' GROUP by hostname ORDER BY size DESC
        """)
    shares_online = cursor.fetchall()

    return render_to_response("pie/pie.html", {'shares': shares, 'shares_online': shares_online,  'gentime': time.time() - generation_started})

        
