# C++23 logger for Windows

Header-only logging for MSVC, applications and injected DLLs. Include
`logger/logger.h`; no library sources need to be compiled.

## Quick start

```cpp
#include "logger/logger.h"

void initialize_logging() { // Outside DllMain.
    log::Config config;
    config.level = log::Level::Debug;
    if (auto result = log::configure(config); !result) {
        // Inspect result.error().code and result.error().system.
        return;
    }

    log::ConsoleConfig console;
    console.auto_alloc = true; // Explicitly request a logger-owned console if needed.
    if (!log::init_console(console)) return;
    if (!log::add_file_sink("logs/module.log")) return;

    log::info("Loaded {} objects", 42);
    log::warn({{"operation", "connect"}, {"attempt", 3}}, "Retrying {}", "database");
}
```

The default mode is **synchronous**. Calls serialize output and flush each record.
No worker, console or files are created implicitly. Use explicit, module-specific
paths: relative paths resolve against the host process working directory.

## DLL lifetime and manual mapping

- Initialize logging outside DllMain and global constructors.
- Synchronous mode needs no worker join. Normal CRT cleanup closes sinks and
  releases a console allocated by the logger, unless free_owned_console is false.
- Stop hooks/producers before unloading their DLL. No logger can keep executing
  code from a module after its memory has been unmapped.
- A manual mapper must initialize the C++ runtime, TLS and exception/unwind support,
  and execute module cleanup. Calling only DllMain is insufficient. No particular
  mapper is assumed or certified by this library.
- Avoid ordinary logging in DllMain, allocator hooks and crash/SEH handlers: it
  uses allocation, locks and I/O. This is not a crash-safe or real-time logger.
- One default logger exists per module, not per process. Do not pass STL objects
  or sink ownership between modules built with incompatible CRT/ABI settings.
- Records own their text, fields and thread name. source_location names belong to
  the calling module; it must remain loaded until its records are processed.
- Late default-facade calls after its destruction are ignored. Explicit Logger
  objects must outlive every caller.

## Asynchronous mode

```cpp
log::Config config;
config.mode = log::Mode::Asynchronous;
config.queue_capacity = 4096;
config.max_queue_bytes = 8 * 1024 * 1024;
config.max_record_bytes = 64 * 1024;
config.max_fields = 32;
config.flush_interval = std::chrono::milliseconds(1000);
config.overflow = log::OverflowPolicy::DropOldest;
if (!log::configure(config)) return;
// Add sinks, then start producers.
```

Choose the mode before the first accepted log attempt. The worker starts lazily.
Before unloading an asynchronous logger's DLL, stop its producers and call
`log::shutdown()` **outside DllMain**. All concurrent shutdown callers wait for
completion. Shutdown drains the queue, flushes and destroys sinks; it is terminal.
Never wait for a worker from DLL_PROCESS_DETACH.

The queue uses a mutex and never waits for free queue space. DropOldest discards
old pending records; DropNewest rejects the new record. Either policy can lose
Error/Critical records under overload. Inspect the return value and statistics.
The byte budget counts text, thread names, field objects, keys and string values;
queue object storage, spare string capacity and the worker's fixed 256-record
batch are additional memory. Caller arguments/custom formatters may allocate.

`flush()` returns a checked result after all prior accepted records have been
processed or accounted as dropped and sink flushes have completed. Concurrent
producers may continue. A failed earlier asynchronous write is reported through
statistics/last_error, even if a later flush succeeds. Flush is not a guarantee
of stable-media persistence or delivery after process termination.

Async sinks flush on the configured timer even while idle. An interval of zero
disables periodic flushing; Error/Critical still request a flush after their batch.

## Levels and filtering

`Trace < Debug < Info < Success < Warn < Error < Critical < Off`.
Use set_level for the logger; use Sink::set_level before transferring a sink
to select its own threshold. Off disables all records.

Ordinary functions filter formatting but C++ evaluates their arguments first.
Use call-site macros to skip argument evaluation and compile out levels:

```cpp
#define LOG_ACTIVE_LEVEL ::log::Level::Info
#include "logger/logger.h"
LOG_DEBUG("{}", expensive_diagnostics()); // No evaluation.
LOG_INFO({{"count", expensive_count()}}, "Snapshot"); // Runtime guard first.
```

LOG_ACTIVE_LEVEL affects **only LOG_TRACE/DEBUG/INFO/SUCCESS/WARN/ERROR/CRITICAL**.
It may differ between translation units because it does not change shared inline
definitions. Function calls such as log::debug use runtime filtering.

```cpp
log::lazy(log::Level::Debug, [&] {
    return std::format("Snapshot: {}", expensive_diagnostics());
});
log::text(log::Level::Info, runtime_text); // Braces are literal text.
```

All seven levels accept either format + arguments or Fields + format + arguments.
The *_ctx compatibility functions accept already-created text and fields.
only_in_debug_build gates records and built-in sink creation in Release (_DEBUG
is absent). It is a runtime policy, not compile-time argument removal.

## Sinks and encoding

- Console output contains optional date/time/source, thread name/ID, level,
  message and fields. Error/Critical use stderr; other levels use stdout.
- Actual Windows consoles use WriteConsoleW; redirected streams receive UTF-8.
  Invalid UTF-8 console input is replaced with U+FFFD.
- Existing console code pages, title, dimensions and output mode are left alone
  by default. modify_host_console explicitly permits title/size/VT changes.
  Owned consoles enable VT when supported. force_ansi permits ANSI explicitly.
- Console allocation is process-wide. The logger restores the process standard
  handle table after allocation and never closes host CRT streams. Only handles
  it opened are closed; only its own console is detached.
- File and JSON sinks rotate by size. A zero max_size disables rotation.
  max_files=0 truncates on rotation and keeps no backups. One oversized record
  is allowed in an empty file. One writer must own an active path and its backups.
- JSON Lines uses UTC timestamps ending in Z, typed values, sequence/thread/source
  metadata and a nested fields object. Duplicate user keys use the last value.
  Non-finite numbers become null; invalid UTF-8 is replaced.

Creation/configuration/flush functions return std::expected<void, Error>.
Logging returns bool: in async mode true means admission, in sync mode a
successful write/flush attempt. False may also mean a filtered record.
Library-owned formatting/allocation failures are caught; evaluation of arguments
and construction of Fields before the call remain the caller's responsibility.

Custom Sink::write/flush exceptions are isolated and counted. Callbacks execute
serially under the sink lock. Logging and blocking control operations into the same
logger are rejected from callbacks; atomic level changes and statistics remain available.
Custom sinks used directly must provide their own synchronization;
destructors must not throw. A sink that blocks forever also blocks shutdown.

## Independent instances and statistics

```cpp
log_core::Logger logger; // No global state or implicit sinks.
auto sink = std::make_unique<log_core::FileSink>(module_path); // May throw.
sink->set_level(log::Level::Warn);
if (!logger.add_sink(std::move(sink))) return;
logger.write<log::Level::Warn>("Operation {} failed", 17);
auto stats = logger.statistics();
auto error = logger.last_error();
```

Statistics include accepted/processed/dropped records, rejected operations, per-level drops,
sink errors, queue payload bytes and high-water mark. Snapshots during production
are observational; accounting identities should be checked after shutdown.
Thread names are UTF-8, limited to 128 bytes at a character boundary.
set_queue_capacity preserves newest pending records and accounts evictions.
Invalid configurations leave existing settings unchanged.

## Build and verification

Use MSVC with a C++23-capable standard library (std::expected/std::formattable),
/EHsc and /utf-8. The checked local toolchain is MSVC 19.51 / v145.
The original .cpp files are compatibility shims and can be removed from builds.
The logger/logger_include.h forwarding header is retained.

```powershell
./tests/run.ps1 -Configuration Debug
./tests/run.ps1 -Configuration Release
./tests/run.ps1 -Configuration Release -Architecture x86
./tests/run.ps1 -Configuration Release -Sanitize
./tests/run.ps1 -Configuration Release -Benchmarks
```

CMake exposes an INTERFACE logger target. The test runner uses VS 2026 Build Tools.
Tests cover queue overflow/resize, delivery barriers, shutdown, reentrancy,
formatting failures, payload limits, idle flush, sink failures, rotation, JSON,
redirected console output and multiple translation units.
The DLL tests include existing host threads, late static destruction and a hidden
logger-owned console that must disappear when its DLL unloads. CMake also verifies
that an invalid format specifier fails compilation.
The reproducible local results are in [benchmarks/README.md](benchmarks/README.md).

## Migration from the previous version

- Logging defaults to synchronous output; choose Mode::Asynchronous explicitly.
- Configure sinks explicitly and inspect returned results.
- Compile-time filtering uses LOG_* macros; ordinary functions use runtime filtering.
- JSON user properties now live inside fields and timestamps use UTC.
