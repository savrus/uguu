from django.conf.urls.defaults import *

urlpatterns = patterns('webuguu.rss.views',
    (r'^$',                  'list'),
    (r'^(?P<type>\w+)/$',    'singletype'),
)

