/* tools/seam_cli.c — Minimal seam-analyze CLI for testing and standalone use.
 *
 * Reads a raw .cfl binary bundle from a file or stdin, runs the causal rule
 * engine, and prints the chain.  Used by ctest to exercise fixture files.
 *
 * Exit codes: 0 = SEAM_OK, 1 = bundle not found / analysis error, 2 = usage.
 *
 * SPDX-License-Identifier: MIT
 */
#include "libseam.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define MAX_BUNDLE (255u * sizeof(cfl_record_t) + sizeof(cfl_bundle_t) + 16u)

int main(int argc, char *argv[])
{
    FILE    *fp;
    uint8_t *buf;
    size_t   len = 0;
    int      rc;
    seam_chain_t chain;

    if (argc != 2 || strcmp(argv[1], "-h") == 0) {
        fprintf(stderr, "usage: seam-analyze <bundle.bin | ->\n");
        return 2;
    }

    fp = (strcmp(argv[1], "-") == 0) ? stdin : fopen(argv[1], "rb");
    if (!fp) {
        fprintf(stderr, "error: cannot open '%s'\n", argv[1]);
        return 2;
    }

    buf = (uint8_t *)malloc(MAX_BUNDLE);
    if (!buf) { if (fp != stdin) fclose(fp); return 2; }

    len = fread(buf, 1, MAX_BUNDLE, fp);
    if (fp != stdin) fclose(fp);

    if (len == 0) {
        fprintf(stderr, "error: empty input\n");
        free(buf);
        return 1;
    }

    rc = seam_analyze((const cfl_bundle_t *)buf, len, &chain);
    if (rc == SEAM_OK) {
        seam_print(&chain, stdout);
    } else {
        fprintf(stderr, "error: seam_analyze returned %d\n", rc);
    }

    free(buf);
    return (rc == SEAM_OK) ? 0 : 1;
}
