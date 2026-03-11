# Log System

Updated: 2026-03-11
Scope: `TDA572/engine/source/runtime/core/log`

## Purpose

This document describes both the current log-system structure and the logging rules that runtime, editor, and upper-layer code should follow.

The current codebase already has a usable logging foundation. The main issue is not missing infrastructure, but inconsistent usage. This document defines the conventions required to make logs easier to search, triage, and maintain.

## Current Structure

The current log system is centered on `LogSystem` and wraps `spdlog` with two logger channels:

- Core logger
- Client logger

It also provides:

- console output
- file output
- macro-based call sites

## Core Responsibilities

### 1. Initialization and Shutdown

The log system is responsible for:

- creating sinks
- creating loggers
- setting level and flush level
- releasing logger resources safely during shutdown

### 2. Core / Client Split

The current system keeps two channels:

- `HYBRID_CORE`
- `HYBRID_CLIENT`

This keeps engine-internal logs separate from upper-layer or application logs.

### 3. Unified Entry Points

Most code should use logging through macros such as:

- `HBD_CORE_DEBUG(...)`
- `HBD_CORE_INFO(...)`
- `HBD_CORE_WARN(...)`
- `HBD_DEBUG(...)`
- `HBD_INFO(...)`
- `HBD_ERROR(...)`

Business code should avoid direct logger-object management.

## Current Implementation State

The current implementation already includes:

- `LogSystem::initialize(...)`
- `LogSystem::shutdown()`
- `LogSystem::core()` / `LogSystem::client()`
- console + file dual output
- level / flush-level setup
- Core / Client dual logger creation
- macro wrappers

## Logging Policy

### 1. Use the existing logging entry points

All new logging should go through the existing macros.

- Engine, runtime, editor, framework, importer, renderer, asset pipeline: use `HBD_CORE_*`
- Game-side or client-facing logic layered above the engine: use `HBD_*`

Do not introduce ad-hoc logging styles such as:

- direct `spdlog::get(...)` calls at call sites
- direct `LogSystem::core()->...` calls in business code
- `printf`, `std::cout`, `std::cerr`, or platform-specific debug print APIs for normal logging

### 2. Keep Core and Client strictly separated

The Core / Client split is not cosmetic. It defines log ownership.

Use `HBD_CORE_*` when the source of truth is engine code:

- runtime lifecycle
- editor services
- rendering backend
- resource system
- scene serialization
- import pipeline

Use `HBD_*` when the source of truth is game logic or higher-level application code:

- gameplay systems
- project-specific rules
- scripting- or tool-layer logic built on top of the engine

If everything is written into the core logger, the split becomes meaningless and debugging ownership gets worse over time.

### 3. One log should describe one event

Each log entry should describe a single event with enough context to understand:

- what happened
- where it happened
- whether it succeeded, degraded, or failed

Avoid splitting one event into many adjacent logs unless each line represents a distinct state transition.

Prefer:

- one summary log for successful initialization
- one error log for a failed operation
- one warning log for a recoverable fallback

Avoid:

- multiple `INFO` lines that only dump surrounding state one by one
- logging the same failure again at every call layer

### 4. Use consistent message structure

New logs should follow this format:

`[Module] event key=value ...`

Examples:

```cpp
HBD_CORE_INFO("[RuntimeResourceSystem] initialized project_root={} assets_root={}",
              projRoot.string(),
              assetsRoot.string());

HBD_CORE_WARN("[AssetMetaStore] meta_parse_failed path={} reason=invalid_json",
              file.string());

HBD_ERROR("[Gameplay] spawn_failed entity={} reason=missing_prefab", entity_id);
```

Rules:

- start with a stable module name in brackets
- use a short event phrase, preferably snake_case or a compact verb phrase
- add the minimum useful context as named fields
- keep field names stable across similar call sites
- prefer structured placeholders over free-form concatenated prose

Recommended field names:

- `path`
- `asset_id`
- `entity_id`
- `scene`
- `reason`
- `result`
- `count`
- `elapsed_ms`

### 5. Keep wording stable and searchable

Logs are operational data, not UI copy.

Use:

- short, repeatable wording
- stable event names
- consistent field names

Avoid:

- conversational wording
- mixed Chinese and English in the same code path
- many synonyms for the same failure mode

The current codebase is predominantly English. New logs should remain in English unless there is a clear project-level decision to change the whole logging language.

## Log Level Rules

### `TRACE`

Use for very high-frequency diagnostic detail that is mainly useful during local debugging.

Typical cases:

- per-frame state traces
- queue churn details
- detailed renderer or importer flow

Requirements:

- must be safe to disable in normal runs
- should not be required to understand normal failures

### `DEBUG`

Preferred for development-oriented diagnostics that are useful but less noisy than `TRACE`.

Typical cases:

- state transitions
- cache hits / misses
- branch decisions during investigation

Available macros:

- `HBD_CORE_DEBUG(...)`
- `HBD_DEBUG(...)`

### `INFO`

Use for important lifecycle milestones and successful operations that matter during normal inspection.

Typical cases:

- subsystem initialized
- scene opened
- import finished
- resource registered

Do not use `INFO` for spammy details that appear every frame or for every trivial branch.

### `WARN`

Use when something went wrong, but the system can still continue, degrade gracefully, or recover.

Typical cases:

- fallback path used
- malformed optional input ignored
- partial scan failure
- duplicate registration ignored

The caller should still be able to continue without treating the current operation as fatal.

### `ERROR`

Use when the current operation failed and the intended result was not achieved.

Typical cases:

- file open failed
- parse failed
- shader compile failed
- required dependency missing

If a function returns failure because of an operation-level problem, `ERROR` is usually the correct level.

### `CRITICAL`

Use only when the process or subsystem cannot reasonably continue, or when data integrity is at serious risk.

Typical cases:

- unrecoverable startup failure
- corruption or invariant violation with no safe fallback
- required global system unavailable and execution must stop

`CRITICAL` should be rare.

## Assertions vs Logs

Assertions and logs serve different purposes.

- use `assert(...)` for programmer errors and invariant checking
- use logs for runtime failures, environmental issues, and operational diagnosis

Do not rely on `assert(...)` alone for failures that may occur in real user runs. If the issue can happen due to input, filesystem state, asset data, or runtime environment, it usually also needs a log.

## Redundancy and Noise Control

### 1. Avoid duplicate logging across layers

The same failure should not be logged in every function that propagates it upward.

Choose one of these boundaries to log:

- the point where the failure is detected and fully understood
- the point where it becomes user-visible or changes control flow materially

If lower layers already log the concrete cause, upper layers should usually add context only when that context changes the diagnosis.

### 2. Rate-limit noisy paths conceptually

Some systems naturally generate noisy events:

- render loop
- file watcher
- asset hot reload
- import queue processing

For these paths:

- prefer `TRACE` or `DEBUG`
- coalesce repeated events where possible
- avoid repeating the exact same warning every frame

## Recommended Defaults

Recommended default runtime behavior:

- `Debug` builds: `debug` or `trace`
- `Release` builds: `info`
- `Shipping` builds: `warn`

Recommended flush behavior:

- flush on `error` and above for normal runs
- increase flushing only when diagnosing logging-loss issues

## Pitfalls and Notes

### 1. Core and Client should stay split

If everything is written into one global logger, logs become much harder to read once the project grows.

Typical failure case:

- engine logs and upper-layer logs mix together
- source ownership becomes unclear during debugging

The current dual-logger structure should stay.

### 2. Initialization and shutdown order matter

Logging is a base dependency for many systems.
If startup or shutdown order becomes loose, failures usually appear as:

- missing early logs
- shutdown-time logger misuse

## Recommended Follow-up Improvements

The current implementation is usable, but these improvements are recommended:

1. Consider module-tag helper macros to reduce repeated `"[Module]"` prefixes.
2. Add build-dependent default log levels.
3. Add optional module-level filtering if log volume grows further.
4. If the editor log-view requirement becomes stable, connect log output to an ImGui panel.

## Quick Checklist

Before adding a new log, check:

- Is this the right ownership channel, `HBD_CORE_*` or `HBD_*`?
- Is the chosen level correct?
- Does one line describe one event?
- Is the module name explicit?
- Are the key context fields present?
- Will this line stay searchable and low-noise over time?
