/* estat.h - exit status values for uguu scanners
 *
 * Copyright 2010, savrus
 * Read the COPYING file in the root of the source tree.
 */

#ifndef ESTAT_H
#define ESTAT_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Scanners are used in a large system and are supposed to exit with
 * specific exit status to integrate with other parts of the system.
 * Each exit status corresponds to specific kind of situation:
 * success, failure or no connection (optional)
 * Lookup process can build chains of scanners and to simplify it's
 * job scanners can exit with RSTAT_NOCONNECT status if further
 * investigation is meaningless (for example, the scanned share
 * has just went offline)
 */

enum {
    ESTAT_SUCCESS = 0,
    ESTAT_FAILURE,
    ESTAT_NOCONNECT
};

#ifdef __cplusplus
}
#endif

#endif /* ESTAT_H */

