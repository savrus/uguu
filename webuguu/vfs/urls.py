from django.conf.urls.defaults import *

urlpatterns = patterns('webuguu.vfs.views',
    (r'^$', 'index'),
    (r'^(?P<network_id>\d+)/$', 'network'),
    (r'^(?P<network_id>\d+)/(?P<share_id>\d+)/$', 'share'),
    (r'^(?P<network_id>\d+)/(?P<share_id>\d+)/(?P<path>.*)/$',
        'share'),
)
