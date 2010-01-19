from django.conf.urls.defaults import *

urlpatterns = patterns('django.views.generic.simple',
    (r'^$',     'direct_to_template', {'template': 'vfs/index.html'}),
)

urlpatterns += patterns('webuguu.vfs.views',
    (r'^net/$',                                             'net'),
    (r'^net/(?P<network>\w+)/$',                            'network'),
    (r'^(?P<proto>\w+)/(?P<hostname>\w+)/$',                'host'),
    (r'^(?P<proto>\w+)/(?P<hostname>\w+)/(?P<port>\d+)/$',  'share'),
    (r'^(?P<proto>\w+)/(?P<hostname>\w+)/(?P<port>\d+)/(?P<path>.*)/$',  'share'),
)

urlpatterns += patterns('django.views.generic.simple',
    (r'^(?P<proto>\w+)/$',      'redirect_to', {'url': '..'}),
)

