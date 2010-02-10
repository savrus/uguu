#!/usr/bin/python
#
# lookup.py - network shares lookup (discovery)
#
# Copyright (c) 2010, Radist <radist.nt@gmail.com>
# Read the COPYING file in the root of the source tree.
#

import psycopg2
import sys
import socket
import re
import collections
from common import connectdb, default_ports, run_scanner, wait_until_next_lookup, wait_until_delete_share
from network import dns_cache, ns_domain, scan_all_hosts

class Share(object):
    def __init__(self, host, proto, port=0, scantype=None):
        self.id = None
        self.host = host
        self.proto = proto
        self.port = port
        self.scantype = scantype
        self.nscache = None # must be set manually
    def ConnectInfo(self):
        port = self.port
        if port == 0:
            port = default_ports[self.proto]
        return (self.nscache(self.host), port)
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
                if self.CheckScantype(scantype) is not None:
                    return self.scantype
            sys.stderr.write("Cann't discover scantype for %s:%s\n" % (self.host, self.ProtoOrPort()))
        else:
            if run_scanner(scantypes[self.proto][self.scantype], self.nscache(self.host),
                           self.proto, self.port, "-l").wait() == 0:
                return self.scantype
        self.scantype = None
        return None

class Lookup(object):
    """
Basic class for lookup engines.
Descedants should not overlap __init__, __del__ methods and
must define __call__ method with self argument only.
"""
    def __init__(self, db, network, params, known_hosts):
        """
db is database connection
network is network name
params is lookup_data for initializing lookup engine
known_hosts is dictionary of "host" : "lookup engine name"
"""
        self.__cursor = db.cursor()
        self.__commit = lambda: db.commit()
        self.__network = network
        self.__params = params
        self.__hosts = known_hosts
        self.__checkshares = collections.defaultdict(dict)
        self.__newshares = collections.defaultdict(dict)
        self.nscache = dns_cache()
        self.default = None
    def __len__(self):
        return len(self.__params)
    def __getitem__(self, key):
        if key in self.__params:
            return self.__params[key]
        else:
            return self.default
    def __del__(self):
        def ListProto(_dict):
            for proto in default_ports.iterkeys():
                yield (proto, _dict[proto])
        def ListPorts(_dict):
            for port in _dict.iterkeys():
                if port not in default_ports:
                    yield (port, _dict[port])
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
            for (host, share) in _sharedict:
                self.__cursor.execute("""
                    INSERT INTO shares (scantype_id, network, protocol,
                        hostname, port, state)
                    VALUES (%(st)s, %(net)s, %(proto)s, %(host)s, %(port)s, 'online')
                    """, {'st': share.scantype, 'net': self.__network,
                          'proto': share.proto, 'host': share.host, 'port': share.port})
        def UpdateHosts(_sharedict):
            sts = collections.defaultdict(list)
            for (host, share) in _sharedict:
                sts[share.scantype].append(share)
            for (st, shares) in sts.iteritems():
                self.__cursor.execute("""
                    UPDATE shares
                    SET scantype_id=%(st)s, last_lookup=now()
                    WHERE share_id IN %(ids)s
                    """, {'st': st, 'ids': tuple(share.id for share in shares)})
        def WalkDict(_dict, routine):
            for (proto, hosts) in ListProto(_dict):
                routine(hosts)
            for (port, hosts) in ListPorts(_dict):
                routine(hosts)
        WalkDict(self.__newshares, RemoveOfflines)
        WalkDict(self.__newshares, InsertHosts)
        self.__commit()
        WalkDict(self.__checkshares, RemoveOfflines)
        WalkDict(self.__checkshares, UpdateHosts)
        self.__commit()
    def AddShare(self, share):
        """
add share with optional scantype detection,
scantype == Ellipsis means "read it from database if possible"
"""
        share.nscache = self.nscache
        self.__cursor.execute("""
            SELECT share_id, scantype_id, last_lookup + interval '%(interval)s' > now()
            FROM shares
            WHERE hostname = '%(host)s' AND
                protocol = '%(proto)s' AND port = '%(port)s'
            """, {'interval': wait_until_next_lookup, 'host': share.host,
                  'proto': share.proto, 'port': share.port})
        data = self.__cursor.fetchone()
        if data is not None:
            share.id = data[0]
            if share.scantype is Ellipsis:
                share.scantype = data[1]
            if data[2] and share.scantype == data[1]:
                return
            self.__checkshares[share.ProtoOrPort()][share.host] = share
        else:
            if share.scantype is Ellipsis:
                share.scantype = None
            self.__newshares[share.ProtoOrPort()][share.host] = share
    def AddServer(self, host, default_shares = True):
        """
add/check server to checklist, try to add default shares if default_shares,
returns permissions to add shares
"""
        if host in self.__hosts and \
           self.__hosts[host] != type(self).__name__:
            return False
        self.__hosts[host] = type(self).__name__
        if default_shares:
            for proto in default_ports.iterkeys():
                self.AddShare(Share(host, proto))
        return True

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
            sys.stderr.write("Errors in network %s configuration\nat lines: %s\n" %
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
        for (section, params) in self.__sections:
            yield lookup_engines[section](db, self.__network, params, known_hosts)


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
            if net == network:
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

class ListStdHosts(Lookup):
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
    def Listing(self, nscache = None):
        """ base listing generator """
        nstype = self['Type']
        valid = ('A', 'AAAA', 'ANY', 'CNAME', 'PTR')
        if nstype not in valid:
            print 'Invalid or missing option Type (valid are %s)' % (valid, )
            raise UserWarning
        self.default = None
        nszone = self['Zone']
        if type(nszone) is not str or \
           nszone[0] == '.' or nszone[-1] == '.':
            print 'Invalid or mission option Zone'
            raise UserWarning
        self.default = ""
        dns = self['DNSAddr']
        self.default = '^.*$'
        keyinclude = re.compile(self['KeyInclude'])
        valinclude = re.compile(self['ValInclude'])
        self.default = '^$'
        keyexclude = re.compile(self['KeyExclude'])
        valexclude = re.compile(self['ValExclude'])
        for (key, val) in ns_domain(nszone, nstype, dns, nscache):
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
        cache = None
        if suffix == self.default:
            cache = self.nscache
        for (key, val) in self.Listing(cache):
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
        for eng in lookup_engines.iterkeys():
            print '%s\t%s' % (eng, lookup_engines[eng].__doc__)
        sys.exit()
        
    try:
        db = connectdb()
    except:
        print "I am unable to connect to the database, exiting."
        sys.exit(1)

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
        netw = sys.argv[3]
    else:
        print 'Invalid command-line, try %s help' % sys.argv[0]
        sys.exit(1)

    for (net, config) in get_networks(db, netw):
        try:
            netconfig = ParseConfig(net, config)
            for lookuper in netconfig(db):
                engine_name = type(lookuper).__name__;
                try:
                    lookuper()
                    del lookuper
                except:
                    sys.stderr.write("Exception in lookup engine %s for network %s\n" % \
                                     (engine_name, net))
            del netconfig
        except:
            sys.stderr.write("Exception during lookup network %s\n" % net)

    db.cursor().execute("DELETE FROM shares WHERE last_lookup + interval %s < now()",
                        (wait_until_delete_share,))
    db.commit()

