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
- [x] Packaging/API: header-only, symmetrical fields, lazy/compile-time filtering.
- [x] Verification: DLL host, multiple TUs, configurations, benchmarks, documentation.

## Validation

- 20 tests pass in Debug x64, Release x64/x86 and Release x64 AddressSanitizer,
  using the explicit MSVC C++23 preview switch.
- DLL tests cover 50 load/unload cycles plus three hidden owned-console cycles per run.
- Compile checks accept a valid format and reject a mismatched format specifier.
- Original Visual Studio solution builds after removal of implementation sources.
- Raw benchmark results and methodology are saved in benchmarks/.
- Benchmark measures 1/2/8 producers, 32/256/4096-byte records, fields, disabled
  logging, null/file/JSON sinks, latency percentiles, throughput and drops.

## Optional follow-ups

ETW/debugger sinks, explicit stack capture and scoped correlation context remain
optional features. Allocations and cross-process file sharing need separate profiling
or a separate transport; they are not claimed by the current benchmark.
