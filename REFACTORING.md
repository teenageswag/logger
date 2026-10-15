# Logger refactoring

## Decisions

- One independent logger per module; no implicit process-wide state or console.
- Synchronous output is the DLL-friendly default; it flushes every record.
- Asynchronous output is opt-in and requires shutdown outside DllMain before unload.
- Manual mappers must initialize the C++ runtime and TLS and execute module cleanup.
- Bounded async queue: 4096 records, 8 MiB payload, 64 KiB per record, 32 fields.
- Drop-oldest by default; drop-newest available. Neither waits for queue space.
- Records own text and fields. Producers and source-location modules must outlive delivery.
- Flush acknowledges delivery attempts, including accounted drops, not stable-media persistence.

## Work

- [x] Runtime: lifecycle, ring invariants, flush barrier, exceptions and regression tests.
- [x] Sinks: I/O errors, console ownership/encoding/fields, JSON, timestamps, rotation.
- [ ] Packaging/API: header-only, symmetrical fields, lazy/compile-time filtering.
- [ ] Verification: DLL host, multiple TUs, configurations, benchmarks, documentation.
