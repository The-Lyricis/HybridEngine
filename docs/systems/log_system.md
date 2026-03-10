# Log System

Updated: 2026-03-10
Scope: `TDA572/engine/source/runtime/core/log`

## Purpose

This document describes the current log-system structure, its capabilities, and how it is used by runtime and editor code.

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

Most code uses logging through macros such as:

- `HBD_CORE_INFO(...)`
- `HBD_CORE_WARN(...)`
- `HBD_INFO(...)`
- `HBD_ERROR(...)`

This keeps business code away from direct logger-object management.

## Current Implementation State

The current implementation already includes:

- `LogSystem::Init(...)`
- `LogSystem::Shutdown()`
- `LogSystem::Core()` / `LogSystem::Client()`
- console + file dual output
- level / flush-level setup
- Core / Client dual logger creation
- macro wrappers

## Current Open Issues

- logging is still synchronous
- runtime config mutation is still limited
- there is no finer module-level filtering policy yet

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

## Future Improvement Plan

I plan to continue with these steps:

1. I will keep the current synchronous path stable before introducing async logging.
2. I will add clearer module-level filtering and configuration controls later.
3. If the editor log-view requirement becomes stable, I will connect the log output to an ImGui panel.
