#!/usr/bin/env python3
"""gen_fixture.py — Generate binary .cfl fixture files for the seam test suite.

Run once to populate test/fixtures/:
    python3 test/gen_fixture.py

Each fixture is a hand-crafted little-endian binary that exercises one specific
case in the seam_analyze() code path.  Tests are deterministic and hardware-
independent — no real MCU required.

Binary layout (mirrors seam_fault_log.h):

  Bundle header (12 bytes, little-endian):
    u32 magic      = 0x4D414553  ("SEAM")
    u8  version    = 1
    u8  n_records
    u16 board_id
    u16 fault_seq
    u16 _pad       = 0

  Per-record (24 bytes, little-endian):
    u8  layer
    u8  event
    u16 seq
    u32 timestamp
    u32 a, b, c, d

SPDX-License-Identifier: MIT
"""

import struct
from pathlib import Path

# ── Format strings ─────────────────────────────────────────────────────────

HEADER_FMT  = "<IBBHHH"          # magic version n_records board_id fault_seq _pad
RECORD_FMT  = "<BBHIIIII"        # layer event seq timestamp a b c d
HEADER_SIZE = struct.calcsize(HEADER_FMT)  # 12
RECORD_SIZE = struct.calcsize(RECORD_FMT)  # 24

assert HEADER_SIZE == 12, f"header size mismatch: {HEADER_SIZE}"
assert RECORD_SIZE == 24, f"record size mismatch: {RECORD_SIZE}"

# ── Constants (mirrors seam_fault_log.h) ───────────────────────────────────

CFL_MAGIC   = 0x4D414553
CFL_VERSION = 1

HW, RTOS, VM, KDI = 0, 1, 2, 3

EV_FAULT_ENTRY    = 0x01
EV_MPU_VIOLATION  = 0x02
EV_IRQ_FIRE       = 0x03
EV_TASK_SWITCH    = 0x10
EV_TASK_BLOCK     = 0x11
EV_QUEUE_FULL     = 0x12
EV_VM_OPCODE      = 0x20
EV_VM_POLICY_FAIL = 0x21
EV_VM_STACK_FAULT = 0x22
EV_KDI_TOKEN_EXP  = 0x30
EV_KDI_THROTTLE   = 0x31
EV_KDI_STATE      = 0x32

# ── Helpers ────────────────────────────────────────────────────────────────

def header(n_records: int, fault_seq: int, board_id: int = 0) -> bytes:
    return struct.pack(HEADER_FMT, CFL_MAGIC, CFL_VERSION,
                       n_records, board_id, fault_seq, 0)

def record(layer: int, event: int, seq: int, ts: int,
           a: int = 0, b: int = 0, c: int = 0, d: int = 0) -> bytes:
    return struct.pack(RECORD_FMT, layer, event, seq, ts, a, b, c, d)

def bundle(records_list: list[bytes], fault_seq: int,
           board_id: int = 0) -> bytes:
    return header(len(records_list), fault_seq, board_id) + b"".join(records_list)

# ── Output directory ───────────────────────────────────────────────────────

OUT = Path(__file__).parent / "fixtures"
OUT.mkdir(exist_ok=True)

fixtures: dict[str, bytes] = {}

# ── 1. normal_kdi_cascade.cfl  (golden case) ──────────────────────────────
# KDI throttle on driver 2 → VM policy fail (same driver) → MPU violation
# → MemManage fault.  This is the canonical causal chain the rule engine
# must detect with high confidence.
fixtures["normal_kdi_cascade.cfl"] = bundle([
    record(KDI, EV_KDI_THROTTLE,   0,   0, a=2, b=0),      # driver 2 throttled
    record(VM,  EV_VM_POLICY_FAIL,  1, 100, a=2, b=0),      # VM policy fail, same actor
    record(HW,  EV_MPU_VIOLATION,   2, 120, a=3, b=1),      # MPU region 3 write
    record(HW,  EV_FAULT_ENTRY,     3, 130, a=0x82, b=0x20000400),  # MemManage
], fault_seq=3)

# ── 2. partial_fill.cfl ────────────────────────────────────────────────────
# Ring is not yet full (8 records < SEAM_RING_SIZE=32).
# Tests that n_records < SEAM_RING_SIZE is handled correctly.
partial_recs = [
    record(KDI, EV_KDI_STATE, i, i * 20, a=i % 3, b=0, c=1)
    for i in range(7)
]
partial_recs.append(record(HW, EV_FAULT_ENTRY, 7, 150, a=0x82, b=0))
fixtures["partial_fill.cfl"] = bundle(partial_recs, fault_seq=7)

# ── 3. bad_magic.cfl ──────────────────────────────────────────────────────
# Magic field is corrupted.  seam_analyze() must return SEAM_ERR_BAD_MAGIC.
_recs = [record(HW, EV_FAULT_ENTRY, 0, 100, a=0x82, b=0)]
_hdr  = struct.pack(HEADER_FMT, 0xDEADBEEF, CFL_VERSION, 1, 0, 0, 0)
fixtures["bad_magic.cfl"] = _hdr + b"".join(_recs)

# ── 4. version_mismatch.cfl ───────────────────────────────────────────────
# Version field is 99.  seam_analyze() must return SEAM_ERR_VERSION.
_recs = [record(HW, EV_FAULT_ENTRY, 0, 100, a=0x82, b=0)]
_hdr  = struct.pack(HEADER_FMT, CFL_MAGIC, 99, 1, 0, 0, 0)
fixtures["version_mismatch.cfl"] = _hdr + b"".join(_recs)

# ── 5. truncated.cfl ──────────────────────────────────────────────────────
# Header claims n_records=20 but the buffer only contains 10 records.
# seam_analyze() must return SEAM_ERR_TRUNCATED.
_recs = [record(HW, EV_FAULT_ENTRY, i, i * 10, a=0x82, b=0) for i in range(10)]
_hdr  = struct.pack(HEADER_FMT, CFL_MAGIC, CFL_VERSION, 20, 0, 19, 0)
fixtures["truncated.cfl"] = _hdr + b"".join(_recs)

# ── 6. wraparound.cfl ─────────────────────────────────────────────────────
# Full ring worth of records (32), simulating a ring that has been running
# long enough for the head to lap the tail.  Records are already linearized
# oldest-first (as seam_dump_bundle() always emits them), so the analyzer
# sees a contiguous sequence starting at seq=0.
wrap_recs = [
    record(RTOS, EV_TASK_SWITCH, i, i * 5, a=i % 4, b=(i + 1) % 4)
    for i in range(30)
]
wrap_recs.append(record(KDI, EV_KDI_THROTTLE, 30, 155, a=1, b=0))
wrap_recs.append(record(HW,  EV_FAULT_ENTRY,  31, 200, a=0x82, b=0x20001000))
fixtures["wraparound.cfl"] = bundle(wrap_recs, fault_seq=31)

# ── Write files ────────────────────────────────────────────────────────────

for name, data in fixtures.items():
    path = OUT / name
    path.write_bytes(data)

print(f"Generated {len(fixtures)} fixture files in {OUT}/")
for name in sorted(fixtures):
    path = OUT / name
    n = (path.stat().st_size - HEADER_SIZE) // RECORD_SIZE
    print(f"  {name:35s}  {path.stat().st_size:4d} B  ({n} records)")
