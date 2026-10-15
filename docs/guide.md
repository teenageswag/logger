# Usage guide

[← README](../README.md)

[Integration](#integration) · [Configuration](#configuration) · [Filtering](#filtering) · [Sinks](#sinks) · [Delivery](#queue-and-delivery) · [DLL lifetime](#dll-lifetime)

## Integration

Add `logger/src` to your include path and include `<logger/logger.h>`.
Use MSVC with C++23 library support for `std::expected` and `std::formattable`,
plus `/EHsc` and `/utf-8`. The verified local compiler is MSVC 19.51 / v145,
using `/std:c++23preview`.

To use the repository through CMake:

```cmake
set(LOGGER_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
add_subdirectory(external/logger)
target_link_libraries(your_target PRIVATE logger)
```

Replace `external/logger` and `your_target` with your checkout path and target.
Set these options before adding the subdirectory. `BUILD_TESTING` is shared
with the parent CMake project; leave it enabled if the parent needs it.

The target supplies the include path, C++23 requirement and MSVC warning/encoding
options. Enable C++ exceptions in the consuming project. The original `.cpp`
files are compatibility shims; header-only consumers do not compile them.
`logger/logger_include.h` remains available as a forwarding header.

## Configuration

Call `log::configure(config)` before starting producers. Choose the mode before
the first accepted log attempt; it cannot change after that. Invalid
configurations leave the existing settings unchanged.

| `log::Config` member | Default | Meaning |
| --- | --- | --- |
| `level` | `Info` | Minimum logger level |
| `mode` | `Synchronous` | Write on the caller or use a worker |
| `only_in_debug_build` | `false` | Suppress records and built-in sink creation when `_DEBUG` is absent |
| `queue_capacity` | `4096` | Maximum pending async records |
| `max_queue_bytes` | `8 MiB` | Maximum queued payload |
| `max_record_bytes` | `64 KiB` | Maximum record payload |
| `max_fields` | `32` | Maximum structured fields per record |
| `overflow` | `DropOldest` | Which records to discard on overflow |
| `flush_interval` | `1000 ms` | Async periodic flush interval; zero disables it |

Valid limits: queue capacity 1–1,048,576; record payload 1 byte–1 MiB; up to
1,024 fields; positive queue byte budget; flush interval from zero through 24 hours.
`log::set_queue_capacity(n)` can resize the queue later. Shrinking keeps the
newest pending records and counts evicted records as drops.

### Results and errors

Creation, configuration and flush APIs return `log::Result`,
an alias for `std::expected<void, log::Error>`:

```cpp
if (auto result = log::add_file_sink("logs/module.log"); !result) {
    const auto code = result.error().code;
    const auto system = result.error().system;
    // Handle the failure through your application's error path.
}
```

Logging calls return `bool`. In async mode, `true` means the record was admitted.
In sync mode it means the write/flush attempt succeeded. `false` can also mean
the record was filtered. A successful call with no sinks has no output destination.

Formatting and allocation failures inside the library are caught. Argument
evaluation and construction of `Fields` happen before entry into the logging
function and can still throw.

## Filtering

The level order is `Trace < Debug < Info < Success < Warn < Error < Critical < Off`.
The logger threshold and each sink's threshold both apply. `Off` disables output.

```cpp
log::set_level(log::Level::Debug);
log::info("Loaded {} objects", 42);
log::warn({{"operation", "connect"}, {"attempt", 3}}, "Retrying {}", "database");
```

Ordinary functions check the level before formatting. To skip argument evaluation
as well, use `LOG_TRACE`, `LOG_DEBUG`, `LOG_INFO`, `LOG_SUCCESS`, `LOG_WARN`,
`LOG_ERROR` or `LOG_CRITICAL`.

Define `LOG_ACTIVE_LEVEL` before including the header to compile out lower macro
calls. It affects only the macros; ordinary functions retain runtime filtering.
The macro threshold may differ between translation units without changing shared
inline definitions.

For deferred text without a macro:

```cpp
log::lazy(log::Level::Debug, [&] {
    return std::format("Snapshot: {}", build_diagnostics());
});
```

For runtime text whose braces must remain literal:

```cpp
log::text(log::Level::Info, "Payload: {ready: true}");
```

All seven level functions support fields. The `*_ctx` compatibility functions
accept already-created text and fields. `only_in_debug_build` is a runtime
policy; it does not remove argument evaluation at compile time.

## Sinks

### Console

`log::init_console(config)` and `log::add_console_sink(config)` are aliases.
No console is created until a console sink is explicitly added.

| `log::ConsoleConfig` member | Default | Purpose |
| --- | --- | --- |
| `auto_alloc` | `true` | Allocate a console when the process has none |
| `title` | Empty | Optional console title |
| `buffer_width`, `buffer_height` | `120`, `30` | Requested console buffer dimensions |
| `enable_colors` | `true` | Color by level when ANSI is supported |
| `enable_backgrounds` | `false` | Use level background colors |
| `force_ansi` | `false` | Explicitly permit ANSI output |
| `modify_host_console` | `false` | Permit title, size and VT changes to an existing console |
| `free_owned_console` | `true` | Detach a logger-owned console during cleanup |
| `show_date`, `show_time`, `show_file` | `true` | Include local date, time and source location |
| `time_format` | `WithMilliseconds` | `HH:MM:SS:MMM`; use `WithoutMilliseconds` for seconds |
| `show_thread_name`, `show_thread_id` | `true`, `false` | Include a set thread name and/or numeric ID |

Set a name on each producing thread with `log::set_thread_name("worker")`.
Names are UTF-8, limited to 128 bytes and truncated at a character boundary.

Actual Windows consoles use `WriteConsoleW`; redirected streams receive UTF-8.
Invalid UTF-8 is replaced with U+FFFD during conversion to console text.
Error and Critical use stderr; other levels use stdout. Colors on an existing
console depend on its VT support unless explicitly enabled through the settings.

Console allocation is process-wide. The logger restores the process standard
handle table after allocation and never redirects or closes the host CRT streams.
Existing console code pages are left alone. By default the logger also preserves
an existing console's title, dimensions and output mode. It closes only handles
it opened and detaches only a console it allocated.

`redirect_streams` is retained for compatibility and has no effect.
`log::remove_console_sink()` removes console sinks.

### Files and rotation

```cpp
log::Result add_rotating_files() {
    if (auto result = log::add_file_sink("logs/module.log", 5 * 1024 * 1024, 2); !result)
        return result;
    return log::add_json_sink("logs/module.jsonl", 10 * 1024 * 1024, 3);
}
```

Text files contain local timestamps, level, source, optional thread name, message
and fields. JSON Lines records use UTC timestamps ending in `Z`, sequence/thread/
source metadata, a message and a nested `fields` object.

Field values preserve strings, booleans, signed/unsigned integers and doubles.
Duplicate user keys use the last value. Non-finite numbers become `null`;
invalid UTF-8 is replaced.

A zero `max_size` disables rotation. `max_files = 0` truncates on rotation and
keeps no backups. One oversized record is allowed in an empty file. Give one
writer ownership of each active path and its backups.

### Custom sinks and thresholds

Derive from `log::Sink` and implement `write(const log_core::LogMessage &)` and
`flush()`. Transfer ownership with `log::add_sink(std::move(sink))`.
Set a sink's threshold before transferring it.

The logger serializes callbacks under its sink lock and isolates/counts exceptions.
Logging and blocking control operations into the same logger are rejected from
callbacks. Atomic level changes and statistics remain available.

Sinks used directly must provide their own synchronization. Destructors must not
throw. A sink that blocks forever also blocks shutdown.

## Queue and delivery

In synchronous mode each call writes and flushes its record under the sink lock.
Async mode uses a bounded queue protected by a mutex and one worker. Callers still
format and construct records; the worker performs sink I/O. The worker starts
lazily on the first log attempt that needs it.

A full queue never waits for free space. `DropOldest` evicts old pending records;
`DropNewest` rejects the incoming record. Either policy can lose Error and
Critical records under load.

The byte budget counts text, thread names, field objects, keys and string values.
Queue object storage, spare string capacity and the worker's fixed 256-record
batch consume additional memory. Caller arguments and custom formatters may
allocate before a record reaches the queue.

### Flush and shutdown

`log::flush()` waits until all prior accepted records have been processed or
accounted as dropped, then flushes the sinks. Concurrent producers may continue.
A failed earlier async write remains visible in statistics and `last_error()`,
even if a later flush succeeds. Flush does not guarantee persistence to physical
storage or delivery after process termination.

Async sinks flush periodically even while idle. An interval of zero disables
periodic flushing; Error and Critical still request a flush after their batch.
The text file sink also checks its own flush interval on writes.

`log::shutdown()` stops admission, drains the queue, flushes and destroys sinks.
Concurrent shutdown callers wait for completion. Shutdown is terminal: configure
and start producers before it, and unload the module after it.

### Statistics

```cpp
auto stats = log::statistics();
auto error = log::last_error();
```

Statistics expose `accepted`, `processed`, `dropped`, `rejected`, `sink_errors`,
`queued`, `queued_bytes`, `high_watermark` and `dropped_by_level`.
`rejected` counts rejected operations, not just records. Snapshots while producers
run are observational; check accounting identities after shutdown.
`log::dropped_count()` and `log::sink_error_count()` are shortcuts.

## Independent instances

The `log::` facade uses one default logger per module. For a separate lifetime
and configuration, create an explicit instance:

```cpp
void write_audit(const std::filesystem::path &path) {
    log::Logger logger;
    auto sink = std::make_unique<log_core::FileSink>(path); // Construction may throw.
    sink->set_level(log::Level::Warn);
    if (!logger.add_sink(std::move(sink)))
        return;
    logger.write<log::Level::Warn>("Operation {} failed", 17);
}
```

An explicit logger starts with no sinks and must outlive every caller.
Late facade calls after default logger destruction are ignored; direct
`log::instance()` access requires a live default instance.

## DLL lifetime

Initialize outside `DllMain` and global constructors. Ordinary logging allocates,
locks and performs I/O; avoid it in allocator hooks and crash/SEH handlers too.

Stop hooks and other producers before unloading their DLL. In synchronous mode,
normal CRT cleanup closes sinks and releases a console owned by the logger
unless `free_owned_console` is false.

For async mode, stop producers and call `log::shutdown()` outside `DllMain`,
before unloading. Never wait for the worker from `DLL_PROCESS_DETACH`.
Records own their text, fields and thread names. The file/function strings in
`std::source_location` belong to the calling module, so it must stay loaded
until its records have been processed.

A manual mapper must set up the C++ runtime, TLS and exception/unwind support,
then execute module cleanup on unload. Calling only `DllMain` is insufficient.
No particular mapper is certified; tests use the Windows loader.

Keep STL objects and sink ownership within modules with compatible CRT/ABI
settings. The logger does not make module code safe to execute after its memory
has been unmapped.

## Verification

Run from the repository root with Visual Studio 2026 Build Tools installed:

```powershell
./tests/run.ps1 -Configuration Debug
./tests/run.ps1 -Configuration Release
./tests/run.ps1 -Configuration Release -Architecture x86
./tests/run.ps1 -Configuration Release -Sanitize
./tests/run.ps1 -Configuration Release -Benchmarks
```

Coverage includes queue overflow and resizing, flush barriers, concurrent shutdown,
reentrancy, formatting failures, payload limits, idle flush, I/O errors, rotation,
JSON parsing, redirected console output and multiple translation units.
Configure-time checks accept a valid format and reject an invalid specifier.

DLL tests cover existing host threads, late static destruction and a hidden
logger-owned console that must disappear on DLL unload.
[Toolchain, verification matrix and local benchmark conditions →](../benchmarks/README.md)

## Migration

- Synchronous output is now the default. Select `Mode::Asynchronous` explicitly.
- Add sinks explicitly and check initialization/configuration results.
- Use `LOG_*` macros for compile-time filtering; ordinary functions filter at runtime.
- JSON user properties live inside `fields`; JSON timestamps use UTC.

[← Back to README](../README.md)
