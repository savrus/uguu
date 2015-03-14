# General introduction #
Database layouts for various uguu versions could be incompatible. If you've installed old version and want to upgrade to the new version, you should whether drop all data (using command `python dbinit.py --dropdb uguu`) and repeat basic server backend setup steps 6 and 7, or update database structure by hand (see appropriate section of this document).

Unfortunately, postgresql don't allow to reorder table columns, so new columns will appear at the end of column list, but it shouldn't affect functionality. If you still want to reorder columns, you could make this in the following five steps:
  1. export all table content into file with `pg_dump -U uguu -W -F p -a --disable-triggers -v -f ./filename.sql -t table_name uguu`
  1. drop all foreign keys referencing the updating table
  1. drop table and recreate it using query from dbinit.py
  1. execute saved dump to fill table data
  1. recreate table indexes and triggers
  1. recreate foreign keys referencing the table

Before update:
  1. Disable cron jobs.
  1. Stop all uguu processes.
  1. Turn down web interface.

After update:
  1. Run scripts manually if it required to fill out the database.
  1. Turn up web interface.
  1. Enable cron jobs.

# Version-by-version update instructions #
## Version-0.2.1 to trunc ##
Pay attention to changes in stored functions. Also, check are there any shares with tree\_id=NULL. You need to manually create a row in trees table and update shares table with the corresponding tree\_id.
As for files table, just add column created.

## Version-0.1 to Version-0.2 ##
Since database scheme have been changed since version 0.1 the easiest way is to drop database and create a new one. Preserving shares and their contents is tough but could be done:
  1. Read misc/save/README to understand how to dump all shares' contents to files and restore back from them
  1. Dump networks, scantypes and shares tables to files using pg\_dump
  1. Dump all share's contents using misc/save/dump.py script.
  1. Drop your database, create a new one using dbinit.py. Whether use the old dbinit.py for dropping database, or drop table filenames and function gfid by hand afterwards.
  1. Clear networks and scantypes tables
  1. Restore saved networks, scantypes and shares tables (you will need to add hash column to shares table before restoring and drop it afterwards)
  1. Execute the following SQL command: "UPDATE shares SET next\_scan=now(), state='online';" this will tell the spider to rescan all shares
  1. Drop indexes associated with paths and files tables: paths\_path, files\_name, files\_tsname, files\_type, files\_size, files\_tsfullpath
  1. Run modified version of spider to restore all shares' contents from files
  1. Create early dropped indexes (paths\_path, files\_name, files\_tsname, files\_type, files\_size, files\_tsfullpath) using queries from dbinit.py
  1. Execute the following SQL command: "UPDATE shares SET next\_scan=last\_scan+interval '12 hours' WHERE last\_scan IS NOT NULL" to recover approximate scanning schedule.

## Version-00 to Version-0.1 ##
Simple way - just execute
```
ALTER TABLE shares ADD COLUMN hostaddr inet;
INSERT INTO scantypes (protocol, scan_command, priority) VALUES
               ('smb', 'smbscan', 0),
               ('ftp', 'ftpscan -c cp1251 -Ra', 10),
               ('ftp', 'ftpscan -c cp1251 -Rp', 0),
               ('ftp', 'ftpscan -c cp1251 -Ma', 0),
               ('ftp', 'ftpscan -c cp1251 -Mp', 1);
```
pinger.py should be runned at least once before starting web-interface to fill hostaddr column.

More advanced way required to keep the column order for shares table. See "General introduction" section. Foreign key paths.paths\_share\_id\_fkey should be deleted before data manipulations with shares table.