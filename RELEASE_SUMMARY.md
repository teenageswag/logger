# Release Summary

## C++23 Async Logger Improvements

This release focuses on safer DLL usage, a cleaner API, stronger lifecycle
management, and more useful structured logging.

### Highlights

- Added the `Critical` log level.
- Reworked levels to use clear names: `Trace`, `Debug`, `Info`, `Success`,
  `Warn`, `Error`, `Critical`.
- Added centralized `log::Config` configuration.
- Added `only_in_debug_build`:
  - Debug builds log normally;
  - Release builds do not create a logger console and skip all log formatting;
  - setting it to `false` enables logging in every build.
- Fixed runtime severity filtering so messages below the configured level are
  rejected before formatting.
- Added safe worker shutdown with queue draining and explicit `flush()` /
  `shutdown()` APIs.
- Fixed thread metadata so names belong to the thread that created the record,
  not the background worker.
- Reworked console ownership for DLL injection scenarios:
  - no `freopen_s` or host stream replacement;
  - no closing of host `stdin/stdout/stderr`;
  - only logger-owned consoles are freed.
- Added optional ANSI foreground and background colors.
- Added typed JSON fields for strings, booleans, integers, and floating-point
  values.
- Hardened rotating file sinks and consolidated rotation logic.
- Added sink exception isolation and sink error statistics.
- Fixed queue capacity validation, resize behavior, and dropped-message
  accounting.
- Added the missing `logger/logger.h` umbrella header and refreshed the sample
  and documentation.

### API migration

```cpp
log::Config config;
config.level = log::Level::Info;
config.only_in_debug_build = true;
log::configure(config);
```

Use `log::Level::Error` and `log::Level::Critical` instead of macro-sensitive
uppercase enum names. The old `logger_include.h` header remains available as a
forwarding compatibility header.

### DLL recommendation

Call `log::shutdown()` before unloading the DLL whenever possible. The logger
will drain pending records and release only resources that it owns.
