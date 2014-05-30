from django.conf.urls.defaults import *

# Uncomment the next two lines to enable the admin:
# from django.contrib import admin
# admin.autodiscover()


urlpatterns = patterns('',
    (r'^$',         include('webuguu.search.urls')),
    (r'^search/',   include('webuguu.search.urls')),
    (r'^light/$',   'webuguu.search.views.light'),
    (r'^vfs/',      include('webuguu.vfs.urls')),
    (r'^faq/',      include('webuguu.faq.urls')),
    (r'^pie/',      include('webuguu.pie.urls')),

    # Uncomment the admin/doc line below and add 'django.contrib.admindocs' 
    # to INSTALLED_APPS to enable admin documentation:
    # (r'^admin/doc/', include('django.contrib.admindocs.urls')),

    # Uncomment the next line to enable the admin:
    # (r'^admin/', include(admin.site.urls)),
)
