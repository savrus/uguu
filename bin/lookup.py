#!/usr/bin/env python
#
# lookup.py - network shares lookup (discovery)
#
# Copyright (c) 2010, Radist <radist.nt@gmail.com>
# Read the COPYING file in the root of the source tree.
#

import psycopg2
import sys
import socket
import string
import re
import collections
import traceback
import datetime
import psycopg2.extensions
from common import connectdb, log, default_ports, run_scanner, wait_until_next_lookup, wait_until_delete_share
from network import dns_cache, ns_domain, scan_all_hosts

class Share(object):
    def __init__(self, host, proto, port=0, scantype=None, _id=None):
        self.id = _id
        self.host = host
        self.proto = proto
        self.port = port
        self.scantype = scantype
        self.nscache = None # must be set manually
    def Addr(self):
        return self.nscache(self.host)
    def ConnectInfo(self):
        port = self.port
        if port == 0:
            port = default_ports[self.proto]
        return (self.Addr(), port)
    def ProtoOrPort(self):
        if self.port == 0:
            return self.proto
        else:
            return self.port
    def CheckScantype(self, scantype = None):
        if scantype is not None:
            self.scantype = scantype
        if self.scantype is None:
            for scantype in scantypes[self.proto].discovery:
                try:
                    if self.CheckScantype(scantype) is not None:
                        return self.scantype
                except StopIteration:
                    break
            log("Cann't discover scantype for %s:%s", (self.host, self.ProtoOrPort()))
        else:
            result = run_scanner(scantypes[self.proto][self.scantype],
                                 self.nscache(self.host), self.proto, self.port, "-l").wait()
            if result == 0:
                return self.scantype
            elif result == 2:
                raise StopIteration
        self.scantype = None
        return None

class Lookup(object):
    """
Basic class for lookup engines.
Descedants should not overlap __init__, __del__ methods and
must define __call__ method with self argument only.
"""
    def __init__(self, db, network, params, known_hosts, nscache):
        """
db is database connection
network is network name
params is lookup_data for initializing lookup engine
known_hosts is dictionary of "host" : "lookup engine name"
"""
        self.__cursor = db.cursor()
        self.__network = network
        self.__params = params
        self.__hosts = known_hosts
        self.__checkshares = collections.defaultdict(dict)
        self.__newshares = collections.defaultdict(dict)
        self.nscache = nscache
        self.default = None
        self.__cursor.execute("""
            SELECT
                share_id,
                scantype_id,
                protocol,
                hostname,
                port,
                last_lookup + interval %(interval)s < now()
            FROM shares
            WHERE network = %(net)s
            """, {'interval': wait_until_next_lookup, 'net': self.__network})
        self.__dbhosts = collections.defaultdict(dict)
        for row in self.__cursor.fetchall():
            share = Share(row[3], row[2], row[4], row[1], row[0])
            self.__dbhosts[share.ProtoOrPort()][share.host] = (share, row[5])
    def __len__(self):
        return len(self.__params)
    def __getitem__(self, key):
        if key in self.__params:
            return self.__params[key]
        else:
            return self.default
    def AddShare(self, share):
        """
add share with optional scantype detection,
scantype == Ellipsis means "read it from database if possible"
"""
        share.nscache = self.nscache
        dbshare = None
        PoP = share.ProtoOrPort()
        if PoP in self.__dbhosts and \
           share.host in self.__dbhosts[PoP]:
            dbshare = self.__dbhosts[PoP][share.host]
            if dbshare[0].proto != share.proto:
                dbshare = None
        if dbshare is not None:
            share.id = dbshare[0].id
            if share.scantype is Ellipsis:
                share.scantype = dbshare[0].scantype
            if dbshare[1] or (share.scantype is not None and share.scantype != dbshare[0].scantype):
                self.__checkshares[PoP][share.host] = share
        else:
            if share.scantype is Ellipsis:
                share.scantype = None
            self.__newshares[PoP][share.host] = share
    def AddServer(self, host, default_shares = True):
        """
add/check server to checklist, try to add default shares if default_shares,
returns permissions to add shares
"""
        if host in self.__hosts and self.__hosts[host] != type(self).__name__:
            return False
        self.__hosts[host] = type(self).__name__
        if default_shares:
            for proto in default_ports.iterkeys():
                self.AddShare(Share(host, proto))
        return True
    def commit(self):
        def RemoveOfflines(_sharedict):
            hosts = frozenset(_sharedict.keys())
            online = set()
            for item in scan_all_hosts([_sharedict[host].ConnectInfo() for host in hosts]):
                online |= self.nscache(None, item[0])
            for host in (hosts - online):
                del _sharedict[host]
            for host in _sharedict.keys():
                if _sharedict[host].CheckScantype() is None:
                    del _sharedict[host]
        def InsertHosts(_sharedict):
            for (host, share) in _sharedict.iteritems():
                self.__cursor.execute("""
                    INSERT INTO shares (scantype_id, network, protocol,
                        hostname, hostaddr, port, state)
                    VALUES (%(st)s, %(net)s, %(proto)s, %(host)s, inet %(addr)s, %(port)s, 'online')
                    """, {'st': share.scantype, 'net': self.__network, 'proto': share.proto,
                          'host': share.host, 'addr': share.Addr(),'port': share.port})
        def UpdateHosts(_sharedict):
            sts = collections.defaultdict(list)
            for (host, share) in _sharedict.iteritems():
                sts[share.scantype].append(share)
            for (st, shares) in sts.iteritems():
                self.__cursor.execute("""
                    UPDATE shares
                    SET scantype_id=%(st)s, last_lookup=now()
                    WHERE share_id IN %(ids)s
                    """, {'st': st, 'ids': tuple(share.id for share in shares)})
        def WalkDict(_dict, routine):
            for portproto in _dict.iterkeys():
                routine(_dict[portproto])
        WalkDict(self.__newshares, RemoveOfflines)
        WalkDict(self.__newshares, InsertHosts)
        WalkDict(self.__checkshares, RemoveOfflines)
        WalkDict(self.__checkshares, UpdateHosts)

class ParseConfig(object):
    """
lookup_config syntax:
#comments
;comments
.COMMENT
Here is
multiline comments
.END
# one engine could be used several times with different settings
# engines are executed in the same order as they are declared
[FirstLookupEngineName]
IntSetting = 0
StringSetting = "somestring"
TupleSetting = ("string value", 1231)
TupleWithSingleIntValue=(123)
[SecondLookupEngineName]
# here are standart settings for all engines:
Include = "if present, include only hosts matching with this regexp"
Exclude = "if present, exclude hosts matching with this regexp"
"""
    def __init__(self, netw, text):
        """ initializes callable Lookup-child generator object """
        self.__sections = []
        self.__network = netw
        errors = set()
        LN = 0
        parse = True
        section = ''
        secdata = {}
        resec = re.compile('^\[(\w+)\]$')
        repar = re.compile('^(?P<name>\w+)\s*=\s*(?P<lst>\()?(?P<value>.*)(?(lst)\))$')
        for line in text.split("\n"):
            LN = LN + 1
            line = string.strip(line)
            if line == "" or line[0] in ';#':
                continue
            if not parse:
                parse = line == ".END"
                continue
            match = repar.match(line)
            if match is not None:
                if section == "" or not self.__addparam(secdata, match):
                    errors.add(LN)
                continue
            match = resec.match(line)
            if match is not None:
                if section == "":
                    if len(secdata) == 0:
                        section = match.group(1)
                    else:
                        errors.add(LN-1)
                else:
                    self.__sections.append((section, secdata))
                    section = match.group(1)
                    secdata = {}
                if section not in lookup_engines:
                    errors.add(LN)
                continue
            if line == ".COMMENT":
                parse = False
            else:
                errors.add(LN)
        else:
            if section == "":
                if len(secdata) > 0:
                    errors.add(LN)
            else:
                self.__sections.append((section, secdata))
        if len(errors) > 0:
            del self.__sections
            log("Errors in network \"%s\" configuration at lines: %s",
                             (self.__network, tuple(errors)))
            raise UserWarning()
    def __addparam(self, data, match):
        def ParseParam(par):
            par = string.strip(par)
            if par[0] == par[-1] == '"':
                return par[1:-1]
            return int(par)
        name = match.group('name')
        if name in data:
            return False
        value = match.group('value')
        try:
            if match.group('lst') is None:
                data[name] = ParseParam(value)
                return True
            else:
                val = []
                for s in re.split('(".*?")|\s*,\s*', value):
                    if s:
                        val.append(ParseParam(s))
                data[name] = tuple(val)
                return True
        except:
            pass
        return False
    def __call__(self, db, known_hosts = dict()):
        nscache = dns_cache()
        for (section, params) in self.__sections:
            yield lookup_engines[section](db, self.__network, params, known_hosts, nscache)


def get_scantypes(db):
    cursor = db.cursor()
    res = dict()
    class ScantypeDict(dict):
        def __init__(self):
            dict.__init__(self)
            self.discovery = list()
    for proto in default_ports.keys():
        cursor.execute("""
            SELECT scantype_id, scan_command, priority>0
            FROM scantypes
            WHERE protocol='%s'
            ORDER BY priority DESC
            """ % proto)
        res[proto] = ScantypeDict()
        for scantype in cursor.fetchall():
            res[proto][scantype[0]] = scantype[1]
            if scantype[2]:
                res[proto].discovery.append(scantype[0])
    return res

def get_networks(db, network = None):
    cursor = db.cursor()
    cursor.execute("SELECT network, lookup_config FROM networks")
    if network is None:
        for net in cursor.fetchall():
            yield (net[0], net[1])
    else:
        for net in cursor.fetchall():
            if net[0] == network:
                yield (net[0], net[1])
                return
        print "Unknown network \"%s\"" % network
        sys.exit(1)

#####################################
### Here are comes Lookup engines

class SkipHosts(Lookup):
    """ preserves hosts in 'list' list from adding into database """
    def __call__(self):
        self.default = tuple()
        hostlist = self['list']
        if type(hostlist) is str:
            hostlist = (hostlist,)
        for host in hostlist:
            self.AddServer(host, False)

class StandardHosts(Lookup):
    """ add standart shares for hosts in 'list' list """
    def __call__(self):
        self.default = tuple()
        hostlist = self['list']
        if type(hostlist) is str:
            hostlist = (hostlist,)
        for host in hostlist:
            self.AddServer(host)

class KeepDBShares(Lookup):
    """ keep scantype from database, 'Count' is share number,
    '0'..'Count-1' are ("host", "proto"[, port(=0)]) """
    def __call__(self):
        self.default = 0
        for i in range(self['Count']):
            item = self[str(i)]
            if type(item) is not tuple or len(item) < 2 or len(item) > 4 or \
               type(item[0]) is not str or type(item[1]) is not str or \
               (len(item) == 3 and type(item[2]) is not int):
                continue
            if not self.AddServer(item[0], False):
                continue
            port = 0
            if len(item) == 3:
                port = item[2]
            self.AddShare(Share(item[0], item[1], port, Ellipsis))

class ManualShares(Lookup):
    """ IMPLICITLY add shares, 'Count' is share number,
    '0'..'Count-1' are ("host", "proto"[, port(=0)[, scantype(=auto)]]) """
    def __call__(self):
        self.default = 0
        for i in range(self['Count']):
            item = self[str(i)]
            if type(item) is not tuple or len(item) < 2 or len(item) > 4 or \
               type(item[0]) is not str or type(item[1]) is not str or \
               (len(item) > 2 and type(item[2]) is not int) or \
               (len(item) == 4 and type(item[3]) is not int):
                continue
            if len(item) == 2:
                share = Share(item[0], item[1])
            elif len(item) == 3:
                share = Share(item[0], item[1], item[2])
            else:
                share = Share(item[0], item[1], item[2], item[3])
            self.AddShare(share)

class DNSZoneListing(object):
    """ required by DNSZoneKeys, DNSZoneValues """
    def Listing(self, nstype = None):
        """ base listing generator """
        if nstype is None:
            nstype = self['Type']
        valid = ('A', 'AAAA', 'ANY', 'CNAME', 'PTR')
        if nstype not in valid:
            print 'Invalid or missing option Type (valid are %s)' % (valid, )
            raise UserWarning
        self.default = None
        nszone = self['Zone']
        if type(nszone) is not str or \
           nszone[0] == '.' or nszone[-1] == '.':
            print 'Invalid or missing option Zone'
            raise UserWarning
        self.default = ""
        dns = self['DNSAddr']
        self.default = '^.*$'
        keyinclude = re.compile(self['KeyInclude'])
        valinclude = re.compile(self['ValInclude'])
        self.default = '^$'
        keyexclude = re.compile(self['KeyExclude'])
        valexclude = re.compile(self['ValExclude'])
        for (key, val) in ns_domain(nszone, nstype, dns).iteritems():
            if keyinclude.match(key) is None or \
               valinclude.match(val) is None or \
               keyexclude.match(key) is not None or \
               valexclude.match(val) is not None:
                continue
            yield (key, val)

class DNSZoneKeys(Lookup, DNSZoneListing):
    """ add all 'Type' keys from 'Zone' zone listing, optionals:
    'DNSAddr', regexps '{Key|Val}{Include|Exclude}', 'Suffix'(="." Zone) """
    def __call__(self):
        self.default = '.' + self['Zone']
        suffix = self['Suffix']
        for (key, val) in self.Listing():
            self.AddServer(key + suffix)

class DNSZoneValues(Lookup, DNSZoneListing):
    """ add all 'Type' values from 'Zone' zone listing, optionals:
    'DNSAddr', regexps '{Key|Val}{Include|Exclude}', changing tuple 'Replace' """
    def __call__(self):
        self.default = ("", "")
        repl = self['Replace']
        if type(repl) is not tuple or len(repl) != 2:
            print 'Invalid Replace option (should be tuple)'
            raise UserWarning
        r = re.compile(repl[0])
        repl = repl[1]
        for (key, val) in self.Listing():
            self.AddServer(r.sub(repl, val))

class FlushDNSCache(Lookup):
    """ just flush DNS-cache (one per network), no parameters """
    def __call__(self):
        self.nscache.__init__()

class DNSZoneToCache(Lookup, DNSZoneListing):
    """ just cache "A" records to prevent single host lookups, params
    are the same as DNSZoneKeys engine's, except 'Type' (not used) """
    def __call__(self):
        self.default = '.' + self['Zone']
        suffix = self['Suffix']
        for (key, val) in self.Listing(nstype = 'A'):
            self.nscache(key + suffix, val)

#TODO: more engines

#####################################
        
if __name__ == "__main__":
    if len(sys.argv) < 2 or len(sys.argv) > 3 or 'help' in sys.argv or '-h' in sys.argv:
        print 'Usage %s [help|confighelp|showengines|showscantypes|shownetworks|showconfig [networkname]|runall|runnet networkname]' % sys.argv[0]
        sys.exit()
    if 'confighelp' in sys.argv:
        print ParseConfig.__doc__
        sys.exit()
        
    lookup_engines = dict([(cl.__name__, cl) for cl in Lookup.__subclasses__()])
    if 'showengines' in sys.argv:
        print 'Known lookup engines:'
        for eng in sorted(lookup_engines.keys()):
            print '%s\t%s' % (eng, lookup_engines[eng].__doc__)
        sys.exit()
        
    try:
        db = connectdb()
    except:
        print "I am unable to connect to the database, exiting."
        sys.exit(1)
    db.set_isolation_level(psycopg2.extensions.ISOLATION_LEVEL_AUTOCOMMIT)

    scantypes = get_scantypes(db)
    if 'showscantypes' in sys.argv:
        for proto in scantypes.iterkeys():
            print 'Protocol "%s" scantypes (detection sequence %s):' % (proto, scantypes[proto].discovery)
            for (i, cmd) in scantypes[proto].iteritems():
                print '%s\t%s' % (i, cmd)
            print ''
        sys.exit()

    if 'shownetworks' in sys.argv:
        print 'Known networks are %s' % [net for (net, config) in get_networks(db)]
        sys.exit()

    if 'showconfig' in sys.argv:
        n = sys.argv.index('showconfig') + 1
        for (net, config) in get_networks(db):
            if n < len(sys.argv) and net != sys.argv[n]:
                continue
            print 'Lookup config for network "%s":\n%s\n' % (net, config)
        sys.exit()

    if len(sys.argv) == 2 and sys.argv[1] == 'runall':
        netw = None
    elif len(sys.argv) == 3 and sys.argv[1] == 'runnet':
        netw = sys.argv[2]
    else:
        print 'Invalid command-line, try %s help' % sys.argv[0]
        sys.exit(1)

    start = datetime.datetime.now()
    for (net, config) in get_networks(db, netw):
        try:
            log("Looking up for network \"%s\"...", net)
            netconfig = ParseConfig(net, config)
            for lookuper in netconfig(db):
                engine_name = type(lookuper).__name__;
                try:
                    lookuper()
                    lookuper.commit()
                except UserWarning:
                    pass
                except:
                    log("Exception in engine '%s' (network \"%s\")",
                                     (engine_name, net))
                    traceback.print_exc()
        except UserWarning:
            pass
        except:
            log("Exception at network \"%s\" lookup", net)
            traceback.print_exc()

    cursor = db.cursor()
    cursor.execute("DELETE FROM shares WHERE last_lookup + interval %s < now() RETURNING tree_id",
                        (wait_until_delete_share,))
    for tree in cursor.fetchall():
        try:
            cursor.execute("DELETE FROM trees WHERE tree_id = %s", (tree[0],))
        except:
            pass
    log("All network lookups finished (running time %s)", datetime.datetime.now() - start)

