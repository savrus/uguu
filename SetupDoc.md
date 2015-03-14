# Introduction #

I think, it's good idea to write detailed installation manual during setting up uguu at melchior. We can post notes etc here and gather all them into release documentation later.


# Installations steps #
## Our configuration ##
  * Debian Lenny (2.6.26-2-amd64)
  * PostgreSQL 8.4.2, installed from sources
  * Python 2.5.2 (repository)
  * django 1.1.1 release
  * Psycopg2 2.0.7-4 (repository)
  * libsmbclient-dev 3.2.5-4 (repository)
  * gcc version 4.3.2
  * GNU Make 3.81
  * Apache 2.2.9 (repository)
  * libapache2-mod-wsgi 2.5-1 (repository)
## Setting up basic server backend ##
  1. Create database and database users
```
postgres=# CREATE ROLE uguu WITH NOSUPERUSER NOCREATEDB NOCREATEROLE LOGIN ENCRYPTED PASSWORD 'password1';
CREATE ROLE
postgres=# CREATE ROLE uguuscript WITH NOSUPERUSER NOCREATEDB NOCREATEROLE LOGIN ENCRYPTED PASSWORD 'password2';
CREATE ROLE
postgres=# CREATE ROLE uguuweb WITH NOSUPERUSER NOCREATEDB NOCREATEROLE LOGIN ENCRYPTED PASSWORD 'password3';
CREATE ROLE
postgres=# CREATE DATABASE uguu WITH OWNER = uguu ENCODING = 'UTF8';
CREATE DATABASE
postgres=# GRANT ALL PRIVILEGES ON DATABASE uguu TO uguu WITH GRANT OPTION;
GRANT
```
  1. Download uguu sources to some folder
```
hg clone https://uguu.googlecode.com/hg/ uguu
cd uguu
hg up version-{version}
```
> > or (if you don't want use Mercurial RCS)
```
wget http://uguu.googlecode.com/files/uguu-{version}.tar.bz2
tar --bzip2 -xf uguu-{version}.tar.bz2
```
  1. Build low-level scanners (using make or msbuild). Scanners binaries are expected to be in the same directory with scripts. In unix use 'make install' command to tell build system do the job for you. In Windows use Install configuration instead of Release ('msbuild /t:Build /p:Configuration=Install uguu.sln'). Or put them there manually. Also, 'make clean' (or 'msbuild /t:Clean') command will not remove scanner binary from bin directory, so you could use it to free some disk space.
  1. Create save dir and setup permissions (also check check that almost python scripts at bin except `common.py` and `dbinit.py` has execute flag).
```
# mkdir bin/save
# chmod -R o-rwx bin
# ls -l bin|grep py$
-rw-r----- 1 uguu uguu  4345 2010-02-13 15:32 common.py
-rw-r----- 1 uguu uguu  7610 2010-02-14 12:15 dbinit.py
-rwxr-x--- 1 uguu uguu 19377 2010-02-14 22:54 lookup.py
-rwxr-x--- 1 uguu uguu  5761 2010-02-14 12:15 network.py
-rwxr-x--- 1 uguu uguu  2327 2010-02-14 23:02 pinger.py
-rwxr-x--- 1 uguu uguu  8013 2010-02-14 13:34 spider.py
```
  1. Configure `common.py` (and optionally `network.py`), use _uguuscript_ as database login
  1. Configure your networks and scantypes in `dbinit.py` (at fill def, not obligatory), execute dbinit.py to initialize database (you could configure networks and scantypes later, but parameters known\_protocols and known\_filetypes from `common.py` must be configured before this)
```
# python dbinit.py --makedb --grant uguu
# dbinit.py --makedb --grant uguu
uguu's password:
R/W user name ('--' to skip) [uguuscript]: 
R/O user name ('--' to skip) [uguuscript]: uguuweb
```
  1. Try the first scripts run: `lookup.py`, than `pinger.py`, than `spider.py`
```
# mkdir ../logs
# time ./lookup.py runall 2>../logs/firstlookup &
...
# time ./pinger.py 2>../logs/firstpinger &
...
# time ./spider.py 2>../logs/firstspider &
```
  1. Setup cron jobs for `lookup.py` (~every 3-4 hours), `pinger.py` (~every 15-20 minutes), `spider.py` (~every 1-3 hours). `spider.py` design permits to run multiple instances, however сreating GIN indexes is very hard task, so it's good idea to lock `spider.py` execution by flock to prevent uncontrolled replication of spider processes. To run multiple instances, just create several jobs. Ideal intervals, maximum spiders count and `wait_until_*` settings in common.py for your network could be selected experimentally. We advice you to run spider not too frequently and not decrease wait\_until\_next\_scan during the first few days after installing, until most shares will be scanned at least once, because differential database updates are much quicker than complete rescans.
We use locking at all scripts:
```
*/15 * *   *   *     flock -w 300 /tmp/uguu-pinger.py.lock0 -c "/path/to/uguu/bin/pinger.py 2>>/path/to/uguu/logs/pinger0.log"
20 */4 *   *   *     flock -w 300 /tmp/uguu-lookup.py.lock0 -c "/path/to/uguu/bin/lookup.py runall 2>>/path/to/uguu/logs/lookup0.log"
10 */2 *   *   *     flock -w 300 /tmp/uguu-spider.py.lock0 -c "/path/to/uguu/bin/spider.py 2>>/path/to/uguu/logs/spider0.log"
10 1-23/2 *   *   *     flock -w 300 /tmp/uguu-spider.py.lock1 -c "/path/to/uguu/bin/spider.py 2>>/path/to/uguu/logs/spider1.log"
```
As for windows, we haven't found how to run console applications in background with built-in scheduler. So, using task scheduler leads to appearing console window for each running task. But you can use [nnCron](http://www.nncron.ru/) or any other scheduler at your option. Here is nncron.tab jobs with functionality similar to the crontab mentioned above:
```
#( pinger0_job
User: "username" SecPassword: "encrypted_password" Domain: "DOMAIN" LogonInteractive
Time: */15 * * * * *
SingleInstance
Action:
StartIn: "path\to\uguu\bin" 
SWHide   IdlePriority
START-APPW: C:\WINDOWS\system32\cmd.EXE /C "path\to\python.exe" pinger.py 2>>..\logs\pinger0.log
)#

#( lookup0_job
User: "username" SecPassword: "encrypted_password" Domain: "DOMAIN" LogonInteractive
Time: 20 */4  * * * *
SingleInstance
Action:
StartIn: "path\to\uguu\bin" 
SWHide   IdlePriority
START-APP: C:\WINDOWS\system32\cmd.EXE /C "path\to\python.exe" lookup.py 2>>..\logs\lookup0.log
)#

#( spider0_job
User: "username" SecPassword: "encrypted_password" Domain: "DOMAIN" LogonInteractive
Time: 10 */2  * * * *
SingleInstance
Action:
StartIn: "path\to\uguu\bin" 
SWHide   IdlePriority
START-APPW: C:\WINDOWS\system32\cmd.EXE /C "path\to\python.exe" spider.py 2>>..\logs\spider0.log
)#

#( spider1_job
User: "username" SecPassword: "encrypted_password" Domain: "DOMAIN" LogonInteractive
Time: 10 1-23/2  * * * *
SingleInstance
Action:
StartIn: "path\to\uguu\bin" 
SWHide   IdlePriority
START-APPW: C:\WINDOWS\system32\cmd.EXE /C "path\to\python.exe" spider.py 2>>..\logs\spider1.log
)#
```
(encrypted\_password could be calculated using 'nncron.exe -ep youpassword' command)

## Setting up web interface ##
  1. Setup permissions (from root)
```
# chgrp -R www-data templates webuguu
# chmod -R o-rwx webuguu
```
  1. Setup `common.py`, use _uguuweb_ as database login
  1. Create directory for django project and make symlinks there, setup permissions
```
# mkdir /home/django-projects/uguu
# cd /home/django-projects/uguu
# ln -s /path/to/uguu/webuguu webuguu
# ln -s /path/to/uguu/templates webuguu-templates
# mkdir logs
# cat >django.wsgi
#/usr/bin/env python
# -*- coding: utf-8 -*-
import os, sys

PROJECT_ROOT = os.path.abspath( os.path.dirname(__file__) )
sys.path.append( PROJECT_ROOT )
os.environ['DJANGO_SETTINGS_MODULE'] = 'webuguu.settings'

import django.core.handlers.wsgi
application = django.core.handlers.wsgi.WSGIHandler()
^D
# chown -R www-data:www-data .
```
  1. Setup DEBUG and TEMPLATE\_DIRS in `settings.py`:
```
DEBUG = False
...
TEMPLATE_DIRS = (
    "/home/django-projects/uguu/webuguu-templates"
)
```
  1. Create virtualhost and restart apache
```
<VirtualHost *:80>
        ServerName 2ch.msu
        ServerAlias 2ch
        ServerAdmin admin@melchior.msu

        ErrorLog    /home/django-projects/uguu/logs/error_log
        CustomLog   /home/django-projects/uguu/logs/access_log common

        WSGIScriptAlias / /home/django-projects/uguu/django.wsgi
</VirtualHost>
```

# Requirements #
  * PostreSQL 8.4 (should we document changes to use with 8.3?)
  * Python >=2.5 <3.0
  * django >=1.1.1
  * Psycopg2
  * nslookup (for Windows) or host (for other platforms), usually shipped with OS
  * libsmbclient (for unix platforms)
  * C/C++ compiler (gcc or msvc), make tool for linux
  * HTTP server

# Performance tweaking #
PostgreSQL default configuration designed to run successfully on slow computers with small RAM amount. So, it should be configured according to your server hardware. Take in account that scanning could results in large commits, so postgresql should be tweaked to maximize both reading and writing performance. Consult PostgreSQL manual or some PostgreSQL tweaking guide for more details.