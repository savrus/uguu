# web interface #
  * search - search page
  * vfs - virtual network browser
  * faq - frequently asked questions page
  * templates - html templates for other parts of web interface

# scripts #
  * pinger.py - update availability info for each share in the database
  * spider.py - for each share in the database invoke appropriate scanner and update this share's files and paths in the database
  * lookup.py - fill database with new shares appeared in the network and remove dead shares
  * dbinit.py - init or drop database, required only during initial setup (and, may be, during update), could be removed from bin directory after setup
  * network.py - some network functions used by other scripts. Most users do not need to run this file
  * common.py - settings

# scanners #
Scanners are ordinary programs that get parameters from command line and print contents of a given share to stdout. Scripts like spider.py and lookup.py do not implement any specific network protocol, they just execute scanners presented in this section to get protocol-specific info. Since spider.py is a python script which works with database and scanners are protocol-specific programs written in C/C++ to work with network, spider.py is often called 'high-level scanner' and scanners below are called 'low-level scanners'
  * smbscan - libsmbclient-based SMB protocol scanner for UNIX systems.
  * smbscanwin - SMB protocol scanner for Windows.
  * ftpscan - crossplatform FTP protocol scanner with flexible listing technique.
  * webdavscan - neon-based WebDAV (HTTP protocol extension) scanner.
  * sqlscan - libpq-based back-from-SQL scanner. This one should not be invoked by spider.py, but can be used to get scanner output back from the database.

# scanning #
As said above scanning is performed by spider.py script. For each share it executes an appropriate scanner and translate it's output into a sequence of SQL commands. All operations with the database are done in a single transaction. If something goes wrong transaction is rollbacked an error reported and spider.py continues scanning with the next share.
Scanning can be done in two modes: non-patching mode and patching mode.

## non-patching mode ##
Generally this is the mode for the first time scanning. spider.py inserts the whole list of share's contents into the database. Sometimes this mode is used for updating share's contents. Then all share's contents are removed from the database first.

## patching mode ##
This mode is generally used for updating. Each uguu low-level scanner supports an option to create a diff between old share's contents and new one. Old contents are read from a file (which is just the output of the previous scanner launch) and new scanner output will consist of two parts: a patch first and then the whole list of contents. The latter is needed to perform scanning in patching mode next time. All scanners outputs are saved by spider.py and correct versioning is enforced using MD5 sum. If patch appears to be bad then fallback to non-patching happens.

Patching mode has been created for the purpose of optimization. Otherwise updating the database (with GIN index on filenames) would have taken forever

# querying #
uguu search in files using postgres full-text search capability. Queries can be extended by options of the form option:argument or for some options option:arg1,arg2,..,argN. Here is the list of options:
  * match:name (default, search in filenames), match:full (search in full path), match:exact (exact filename search)
  * max:_size_, min:size (size constraints, size argument is a decimal number with specifier _b_, _k_, _m_, _g_ or _t_)
  * type:_type_ (_type_ of the file based on it's suffix. Suffixes should be configured in common.py, by default _dir_, _video_, _audio_, _archive_, _cdimage_, _exe_, _lib_, _script_, _image_, _document_ are supported)
  * proto:_protocol_ (share's protocol: smb, ftp, http)
  * host:_hostname_
  * port:_port_
  * network:_network_
  * avl:_availability_ (_online_ or _offline_)
  * order:_list of orderings_ (order can be one of the avl, uptime, sharesize, name, scan, net, proto, type, host, size, avl.d, uptime.d, sharesize.d, name.d, scan.d, net.d, proto.d, type.d, host.d, size.d or the comma-separated combination; suffix ".d" means "desc")
  * out:_format_ (default html, or rss)