LAN indexing tool with web interface. Supports SMB, FTP and WebDAV resources. Works on both Windows and Linux. Written in C, C++, python. Uses [postgres](http://www.postgresql.org/), [django](http://www.djangoproject.com/), [neon](http://www.webdav.org/neon/), [libsmbclient](http://www.samba.org/) (linux only).

Uguu has several important features:
  * flexible network configuration
  * automatic resources list management: non-empty resources of each protocol are added to the database, resources that are no more available for too long are removed.
  * a resource's contents updating process is hidden from user meaning the user can search and navigate the resource even while it's rescanning is performed
  * recursive symlink detection
  * on each rescan only changes of resource's contents are committed to the database meaning most rescan cycles do not touch the database at all or perform very little change.
  * several scanners may run in parallel.
  * unicode support


<br>

THIS SOFTWARE HAS BEEN CREATED WITH THE SOLE GOAL TO WRITE A SOFTWARE TECHNICALLY COMPETITIVE WITH OTHER SOFTWARE OF SUCH CLASS. WE BELIEVE NOWADAYS THERE ARE PLENTY OF FREE SOFTWARE, AUDIO FILES, VIDEO FILES AND DOCUMENTS WHICH MAY CONTAIN STUDY MATERIALS AS WELL. THIS SOFTWARE UNDER ANY CONDITIONS MUST NOT BE USED TO HELP ILLEGAL DISTRIBUTION OF COPYRIGHTED MATERIAL IN THE AREA IT IS USED.