# Local verification

Date: 2026-10-15. Host: Windows x64, Intel Core i3-12100F.
Toolchain: MSVC 19.51.36256 / v145, Windows SDK 10.0.26100, C++23 preview mode.
Benchmarks use Release /O2 with the dynamic CRT; AddressSanitizer is tested separately.

## Tests

| Configuration | Result |
| --- | --- |
| Debug x64 | 20/20 passed |
| Release x64 | 20/20 passed |
| Release x86 | 20/20 passed |
| Release x64 + AddressSanitizer | 20/20 passed |

Each suite includes 50 normal DLL load/unload cycles (sync and async),
three hidden owned-console cycles, an existing host thread, late static logging,
a three-translation-unit executable, a deterministic queue model, overflow/resize,
busy flush, concurrent shutdown, formatting failures, limits, timer flush, I/O errors,
rotation, JSON parsing and redirected console output.

Configure-time checks accept a valid format string and reject a mismatched specifier.
The original Visual Studio solution was also built independently.

Manual mapping was not exercised: no mapper was provided. CRT/TLS/unwind setup
and cleanup are requirements of the mapper. The tests exercise the Windows loader.
AddressSanitizer checks memory safety; it does not prove the absence of data races.

## Benchmark

Source: [tests/benchmark.cpp](../tests/benchmark.cpp).
Raw results: [msvc-v145-x64.csv](msvc-v145-x64.csv).

Run:

```powershell
./tests/run.ps1 -Configuration Release -Benchmarks
```

Each producer makes 5,000 calls. Message sizes are 32/256/4096 bytes.
The async queue has 8,192 slots and a 64 MiB payload budget. Five-field cases
include integers, a boolean, a double and a string. The null sink does no I/O.
Producer percentiles include formatting, argument/field construction and admission;
processed throughput includes the final drain/flush. Attempt throughput also counts drops.

Selected 256-byte cases:

| Mode / sink | Producers | Fields | Producer p99, us | Processed records/s | Dropped |
| --- | ---: | ---: | ---: | ---: | ---: |
| sync / null | 1 | 0 | 0.5 | 2,859,266 | 0 |
| sync / null | 8 | 0 | 25.4 | 2,656,307 | 0 |
| async / null | 1 | 0 | 1.5 | 1,485,133 | 0 |
| async / null | 8 | 0 | 37.1 | 2,196,207 | 3,933 |
| async / file | 1 | 5 | 1.2 | 572,383 | 0 |
| async / json | 1 | 5 | 4.4 | 317,902 | 0 |

This is a short local run without CPU affinity, warm-up control or allocation
instrumentation. Timer quantization is visible in sub-microsecond samples.
It is a reproducible smoke benchmark, not an SLA or a comparison against the old version.

The eight-producer async cases can fill the queue. Drops are reported separately
from completed throughput; a higher admission rate must not be read as higher delivery.
Queue policy and capacity should be tuned against the real host workload.
