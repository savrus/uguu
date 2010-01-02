/* smbscan - smb scanner
 *
 * Copyright 2009, savrus
 * All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Uguu Team nor the names of its
 *     contributors may be used to endorse or promote products derived from
 *     this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dt.h"
#include "smbwk.h"

static void usage(char *binname, int err)
{
    fprintf(stderr, "Usage: %s [-f out] host\n", binname);
    fprintf(stderr, "out:\n");
    fprintf(stderr, "\tfull - print full paths\n");
    fprintf(stderr, "\tsimplified - print only id of a path\n");
    fprintf(stderr, "\tfilesfirst - simplified with filis printed first\n");
    exit(err);
}

int main(int argc, char **argv)
{
    struct dt_dentry d = {DT_DIR, "", 0, NULL, NULL, NULL, 0};
    struct smbwk_dir curdir;
    int out = DT_OUT_SIMPLIFIED;
    char *host;

    if (argc < 2)
        usage(argv[0], EXIT_FAILURE);
    
    host = argv[1];

    if (strcmp(argv[1], "-f") == 0) {
        if (argc < 4)
            usage(argv[0], EXIT_FAILURE);

        host = argv[3];
        if (strcmp(argv[2], "full") == 0)
            out = DT_OUT_FULL;
        else if (strcmp(argv[2], "simplified") == 0)
            out = DT_OUT_SIMPLIFIED;
        else if (strcmp(argv[2], "filesfirst") == 0)
            out = DT_OUT_FILESFIRST;
        else {
            fprintf(stderr, "Unknown output format\n");
            usage(argv[0], EXIT_FAILURE);
        }
    }

    smbwk_init_curdir(&curdir, host);
    
    switch(out) {
        case DT_OUT_FILESFIRST:
            dt_singlewalk(&smbwk_walker, &d, &curdir, out);
            break;
        default:
            dt_mktree(&smbwk_walker, &d, &curdir, out);
            dt_printtree(&d, out);
            dt_free(&d);
            break;
    }

    smbwk_fini_curdir(&curdir);

    return 0;
}

