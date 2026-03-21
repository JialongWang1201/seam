/* seam_print.c — Human-readable causal chain renderer
 *
 * SPDX-License-Identifier: MIT
 */
#include "libseam.h"
#include <stdio.h>

static const char *layer_name(uint8_t layer)
{
    switch (layer) {
        case CFL_LAYER_HW:   return "HW  ";
        case CFL_LAYER_RTOS: return "RTOS";
        case CFL_LAYER_VM:   return "VM  ";
        case CFL_LAYER_KDI:  return "KDI ";
        default:             return "????";
    }
}

void seam_print(const seam_chain_t *chain, FILE *fp)
{
    fprintf(fp,
        "\nCAUSAL CHAIN (confidence: %d%%)\n"
        "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n",
        chain->verdict_confidence);

    int depth = (int)chain->depth;
    for (int i = 0; i < depth; i++) {
        const seam_node_t  *node = &chain->chain[i];
        const cfl_record_t *r    = node->record;
        int offset = i - (depth - 1); /* T-N ... T-0 */

        if (offset == 0) {
            fprintf(fp, "[T-0] FAULT ANCHOR "
                        "──────────────────────────────────────\n");
        } else {
            fprintf(fp, "[T%d] %s  ev=0x%02x seq=%-5u  %s\n",
                    offset,
                    layer_name(r->layer),
                    r->event,
                    r->seq,
                    node->cause);
        }
    }

    if (chain->truncated)
        fprintf(fp, "      [... chain truncated at %d nodes]\n", SEAM_CHAIN_MAX);

    fprintf(fp,
        "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n"
        "VERDICT: %s\n\n",
        chain->verdict);
}
