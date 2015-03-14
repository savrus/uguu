# Introduction #

Database design should allow efficient queries for:
  1. Updating share directory tree in a scanner that gets data in 'reverse' format from low-level scanner
  1. VFS browse. Browser should be able to get full path from user, not just directory id. However for navigation browser should use ids for speed. So we must support both.
  1. Search queries. Search by file names, file types, full path, whatsoever

Professional postgres user give us an advice not to use short tables which can be easily implemented in python scripts as python native objects.

# DDL #
Uguu uses 3 custom enum types: filetype (includes file types also described in configuration), proto (known protocols, currently are 'ftp', 'smb' and 'http' for webdav) and availability ('online' and 'offline').

## networks ##
| **Column name** | **Data type** | **Misc options** | **Description** |
|:----------------|:--------------|:-----------------|:----------------|
| network | varchar(32) | primary key | Network display name |
| lookup\_config | text | NULL | Configuration for lookup shares procedure |


## scantypes ##
| **Column name** | **Data type** | **Misc options** | **Description** |
|:----------------|:--------------|:-----------------|:----------------|
| scantype\_id | serial | primary key |  |
| scan\_command | text |  | Command to scan the share |
| protocol | proto |  |  |
| priority | smallint | default (-1) | Priority for scantype discover process, only scantypes with priority >0 are used for auto discover |

## shares ##
| **Column name** | **Data type** | **Misc options** | **Description** |
|:----------------|:--------------|:-----------------|:----------------|
| share\_id | serial | primary key |  |
| tree\_id | integer | NULL, REFERENCES trees ON DELETE RESTRICT | Index of share's tree |
| scantype\_id | integer | REFERENCES scantypes ON DELETE RESTRICT | Which scanner is capable of scanning this share |
| network | varchar(32) | REFERENCES networks ON DELETE CASCADE | Network host belongs to |
| protocol | proto |  | Share protocol |
| hostname | varchar(64) |  | Full host name or ip address |
| hostaddr | inet | NULL | Host's ip address |
| port | smallint | >=0, <65536, default 0 | Share port or 0 for the default port |
| state | availability | default 'offline' | Whether share is on-line now |
| size | bigint |  | Total size of files within share |
| last\_state\_change | timestamp |  | Last time when on-line state changed |
| last\_scan | timestamp |  | Time of the last successful scan |
| next\_scan | timestamp |  | Planned time of the next scan (host will not be scanned before this time) |
| last\_lookup | timestamp | NULL | Time of last successful lookup including timestamp discovery |
| username | varchar(32) | NULL | not implemented yet |
| password | varchar(32) | NULL | not implemented yet |

## trees ##
| **Column name** | **Data type** | **Misc options** | **Description** |
|:----------------|:--------------|:-----------------|:----------------|
| tree\_id | serial | primary key |  |
| share\_id | integer | REFERENCES shares ON DELETE CASCADE | The share tree belongs to |
| hash | varchar(32) | NULL | Hash of saved scan for consistency check before patching update |

## paths ##
| **Column name** | **Data type** | **Misc options** | **Description** |
|:----------------|:--------------|:-----------------|:----------------|
| tree\_id | integer | REFERENCES trees ON DELETE CASCADE | Tree for share where path is present |
| treepath\_id | integer |  | Path id within tree (unique for the same tree\_id) |
| parent\_id | integer | NULL | Parent path id, 0 for share root |
| parentfile\_id | integer | NULL | File id for a path in it's parent file list |
| path | text |  | Full path without trailing slash (except for /) |
| items | integer |  | Total objects (files, directory) count |
| size | integer |  | Total size of this path |
|  | _primary key_ | share\_id, sharepath\_id |  |

## files ##
| **Column name** | **Data type** | **Misc options** | **Description** |
|:----------------|:--------------|:-----------------|:----------------|
| file\_id | bigserial | primary key | not used directly by uguu |
| tree\_id | integer <td> REFERENCES paths ON DELETE CASCADE <table><thead><th> Tree for share which file was found on </th></thead><tbody>
<tr><td> treepath_id </td><td> integer </td><td> Path where file was found </td></tr>
<tr><td> pathfile_id </td><td> integer </td><td>  </td><td> Object (file or directory) id within path </td></tr>
<tr><td> treedir_id </td><td> integer </td><td> 0 </td><td> 0 for files, corresponding treepath_id for dirs </td></tr>
<tr><td> size </td><td> bigint </td><td>  </td><td> File size/total size of all files inside directory </td></tr>
<tr><td> name </td><td> text </td><td>  </td><td> Name of the file or directory </td></tr>
<tr><td> type </td><td> filetype </td><td>  </td><td> Type of file </td></tr>
<tr><td> tsname </td><td> tsvector </td><td> gin index </td><td> Indexing vector for file name </td></tr>
<tr><td> tspath </td><td> tsvector </td><td> gin index </td><td> Indexing vector for full path </td></tr>
<tr><td> created </td><td> timestamp </td><td> default now() </td><td> When file first appeared </td></tr></tbody></table>

<h1>Comments</h1>
parent_id, parentfile_id, paths.size are just copies from a files table (referenced by share_id, sharepath_id = parent_id). Yes, I know.<br>
filenames table was dropped during speedup process, names are moved to the files column