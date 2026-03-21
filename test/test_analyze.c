/* test_analyze.c — seam analysis engine tests
 *
 * No external test framework. Returns 0 on pass, 1 on any failure.
 * Build: cmake -B build && cmake --build build && ctest --test-dir build
 *
 * SPDX-License-Identifier: MIT
 */
#include "libseam.h"
#include "seam_fault_log.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define PASS "\033[32mPASS\033[0m"
#define FAIL "\033[31mFAIL\033[0m"

static int failures = 0;

#define ASSERT(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "  " FAIL "  %s  (line %d)\n", (msg), __LINE__); \
        failures++; \
    } else { \
        fprintf(stdout, "  " PASS "  %s\n", (msg)); \
    } \
} while(0)

/* ── Fixture helpers ────────────────────────────────────────────────────── */

typedef struct {
    cfl_bundle_t     hdr;
    cfl_record_t     recs[16];
} fixture_t;

static void fixture_init(fixture_t *f)
{
    memset(f, 0, sizeof(*f));
    f->hdr.magic   = CFL_MAGIC;
    f->hdr.version = CFL_VERSION;
}

static void fixture_add(fixture_t *f, uint8_t layer, uint8_t ev,
                        uint32_t ts, uint32_t a, uint32_t b)
{
    uint8_t i = f->hdr.n_records++;
    f->recs[i].layer     = layer;
    f->recs[i].event     = ev;
    f->recs[i].seq       = i;
    f->recs[i].timestamp = ts;
    f->recs[i].a         = a;
    f->recs[i].b         = b;
}

static size_t fixture_len(const fixture_t *f)
{
    return sizeof(cfl_bundle_t) + f->hdr.n_records * sizeof(cfl_record_t);
}

/* ── Test cases ─────────────────────────────────────────────────────────── */

static void test_bad_magic(void)
{
    printf("\n[bad magic]\n");
    fixture_t f; fixture_init(&f);
    f.hdr.magic = 0xDEADBEEF;
    seam_chain_t chain;
    int rc = seam_analyze((cfl_bundle_t *)&f, fixture_len(&f), &chain);
    ASSERT(rc == SEAM_ERR_BAD_MAGIC, "returns SEAM_ERR_BAD_MAGIC");
}

static void test_version_mismatch(void)
{
    printf("\n[version mismatch]\n");
    fixture_t f; fixture_init(&f);
    f.hdr.version = 99;
    seam_chain_t chain;
    int rc = seam_analyze((cfl_bundle_t *)&f, fixture_len(&f), &chain);
    ASSERT(rc == SEAM_ERR_VERSION, "returns SEAM_ERR_VERSION");
}

static void test_truncated(void)
{
    printf("\n[truncated bundle]\n");
    fixture_t f; fixture_init(&f);
    fixture_add(&f, CFL_LAYER_HW, CFL_EV_FAULT_ENTRY, 100, 0x82, 0x20000400);
    f.hdr.fault_seq = 0;
    seam_chain_t chain;
    /* Pass only the header, no records */
    int rc = seam_analyze((cfl_bundle_t *)&f,
                          sizeof(cfl_bundle_t) - 1, &chain);
    ASSERT(rc == SEAM_ERR_TRUNCATED, "returns SEAM_ERR_TRUNCATED");
}

static void test_no_fault_anchor(void)
{
    printf("\n[missing fault anchor]\n");
    fixture_t f; fixture_init(&f);
    fixture_add(&f, CFL_LAYER_HW, CFL_EV_IRQ_FIRE, 10, 5, 1);
    f.hdr.fault_seq = 99; /* no record with seq=99 */
    seam_chain_t chain;
    int rc = seam_analyze((cfl_bundle_t *)&f, fixture_len(&f), &chain);
    ASSERT(rc == SEAM_ERR_NO_FAULT_ANCHOR, "returns SEAM_ERR_NO_FAULT_ANCHOR");
}

static void test_kdi_cascade(void)
{
    printf("\n[KDI throttle → VM policy fail → MemManage fault]\n");
    fixture_t f; fixture_init(&f);

    /* T=0: KDI throttle hits on driver 2 */
    fixture_add(&f, CFL_LAYER_KDI, CFL_EV_KDI_THROTTLE, 0,   2, 0);
    /* T=100: VM policy fail on driver 2 (within 500 tick window) */
    fixture_add(&f, CFL_LAYER_VM,  CFL_EV_VM_POLICY_FAIL, 100, 2, 0);
    /* T=120: MPU violation */
    fixture_add(&f, CFL_LAYER_HW,  CFL_EV_MPU_VIOLATION, 120, 3, 1);
    /* T=130: MemManage fault anchor */
    fixture_add(&f, CFL_LAYER_HW,  CFL_EV_FAULT_ENTRY,  130, 0x82, 0x20000400);
    f.hdr.fault_seq = 3; /* seq of the fault entry */

    seam_chain_t chain;
    int rc = seam_analyze((cfl_bundle_t *)&f, fixture_len(&f), &chain);

    ASSERT(rc == SEAM_OK,      "analyze returns SEAM_OK");
    ASSERT(chain.depth >= 2,   "chain has at least 2 nodes");
    ASSERT(chain.verdict != NULL, "verdict is set");
    ASSERT(chain.verdict_confidence > 0, "confidence > 0");

    printf("  verdict: %s (%d%%)\n",
           chain.verdict, chain.verdict_confidence);
    seam_print(&chain, stdout);
}

static void test_partial_fill(void)
{
    printf("\n[partial fill — fewer records than SEAM_RING_SIZE]\n");
    fixture_t f; fixture_init(&f);
    fixture_add(&f, CFL_LAYER_HW, CFL_EV_MPU_VIOLATION, 0,  3, 1);
    fixture_add(&f, CFL_LAYER_HW, CFL_EV_FAULT_ENTRY,  10, 0x82, 0);
    f.hdr.fault_seq = 1;

    seam_chain_t chain;
    int rc = seam_analyze((cfl_bundle_t *)&f, fixture_len(&f), &chain);
    ASSERT(rc == SEAM_OK,    "partial fill: analyze returns SEAM_OK");
    ASSERT(chain.depth >= 1, "partial fill: chain has at least 1 node");
}

/* ── main ───────────────────────────────────────────────────────────────── */
int main(void)
{
    printf("seam test suite\n");
    printf("═══════════════\n");

    test_bad_magic();
    test_version_mismatch();
    test_truncated();
    test_no_fault_anchor();
    test_kdi_cascade();
    test_partial_fill();

    printf("\n─────────────────────────────\n");
    if (failures == 0)
        printf(PASS "  All tests passed.\n\n");
    else
        printf(FAIL "  %d test(s) failed.\n\n", failures);

    return failures ? 1 : 0;
}
