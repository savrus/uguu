# webuguu.rss.views - rss view for django framework
#
# Copyright 2010, savrus
# Read the COPYING file in the root of the source tree.
#

from django.http import HttpResponse
from django.utils import feedgenerator
from django.utils.http import urlquote
from django.core.urlresolvers import reverse
from django.shortcuts import render_to_response
from webuguu.common import connectdb, known_filetypes, rss_items

def alltypes(request):
    feed = feedgenerator.Rss201rev2Feed(
        title = u"New Files",
        link = reverse('webuguu.search.views.search'),
        description = "New files appeared in the network",
        language=u"en")
    try:
        db = connectdb()
    except:
        return HttpResponse(feed.writeString('UTF-8'))
    cursor = db.cursor()
    cursor.execute("""
        SELECT name FROM filenames ORDER BY filename_id DESC LIMIT %(l)s
        """, {'l': rss_items})
    for file in cursor.fetchall():
        feed.add_item(
            title = file['name'],
            link = "http://" + request.META['HTTP_HOST']
                   + reverse('webuguu.search.views.search')
                   + "?q=" + file['name'] + " match:exact",
            description = file['name'])
    return HttpResponse(feed.writeString('UTF-8'))

def singletype(request, type):
    if type == "all":
        return alltypes(request)
    feed = feedgenerator.Rss201rev2Feed(
        title = u"new %s files" % type,
        link = reverse('webuguu.search.views.search'),
        description = "New %s files appeared in the network" % type,
        language=u"en")
    if type not in known_filetypes:
        return HttpResponse(feed.writeString('UTF-8'))
    try:
        db = connectdb()
    except:
        return HttpResponse(feed.writeString('UTF-8'))
    cursor = db.cursor()
    cursor.execute("""
        SELECT name FROM filenames
        WHERE type = %(t)s
        ORDER BY filename_id DESC LIMIT %(l)s
        """, {'t': type, 'l': rss_items})
    for file in cursor.fetchall():
        feed.add_item(
            title = file['name'],
            link = "http://" + request.META['HTTP_HOST']
                   + reverse('webuguu.search.views.search')
                   + "?q=" + file['name'] + " match:exact",
            description = file['name'])
    return HttpResponse(feed.writeString('UTF-8'))

def list(request):
    rsses = tuple(known_filetypes) + ('all',)
    return render_to_response('rss/list.html', {'rsses': rsses})
