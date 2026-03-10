# Event System

Updated: 2026-03-10
Scope: `TDA572/engine/source/runtime/core/event`, `TDA572/engine/source/runtime/modules/input`

## Purpose

This document describes the current event-system structure, its dispatch path, and how it connects to the input system, window system, and LayerStack.

## Current Structure

The current event system is built from four main parts:

- event base types and event definitions
- `EventDispatcher`
- `Layer` / `LayerStack` propagation
- `InputLayer` input-state sampling

## Core Responsibilities

### 1. Event Model

The event system provides a unified event abstraction based on:

- `EventType`
- `EventCategory`
- `Handled`

This model is used to represent window events, application events, and input events in one consistent way.

### 2. Dispatcher

`EventDispatcher` takes a generic `Event&` and routes it to a typed handler when the type matches.

This solves two problems:

- the engine can pass events around through one base type
- each subsystem can still handle only the event types it cares about

### 3. Layer Propagation

Events propagate through `LayerStack` in reverse order.

Current rule:

- upper layers receive the event first
- once a layer marks the event as handled
- propagation stops

This matches the expected editor behavior where overlays and UI should intercept input first.

### 4. Input Sampling

`InputLayer` samples input state from incoming events.

Important rule:

- input-state sampling must not depend on whether the event is marked as handled
- even if UI consumes the event, the frame input snapshot should stay complete

## Current Implementation State

The current system already includes:

- event base types and categories
- window and input event types
- `EventDispatcher`
- reverse `LayerStack` dispatch
- `InputLayer` input sampling
- the main platform-to-engine event path

Current path summary:

1. GLFW callback fires
2. the callback builds an engine event object
3. the event is sent to `HybridEngine::onEvent`
4. `InputLayer` samples it first
5. `LayerStack` receives it after that

## Current Open Issues

- `Layer` lifetime boundaries around `onAttach / onDetach` still need to stay explicit and clean
- dispatch is still synchronous and there is no standalone event queue layer
- resize and other system-level events reach the engine, but cross-system follow-up rules should stay documented clearly

## Pitfalls and Notes

### 1. Input sampling cannot depend on handled state

If input state is updated only after UI or another layer decides not to consume the event, frame input becomes incomplete.

Typical failure case:

- UI consumes a mouse or keyboard event
- runtime input state misses part of that frame

Current fix and rule:

- `InputLayer` samples first
- business dispatch happens after that

### 2. Event flow and state flow are different things

Events are transient messages.
Input state is a frame snapshot.
They should not be designed as the same layer.

## Future Improvement Plan

I plan to continue with these steps:

1. I will keep the resize and other system-level event follow-up behavior documented more explicitly.
2. I will only introduce an event queue if the synchronous path becomes a real bottleneck or design problem.
3. I will continue to keep input sampling and business dispatch separated so UI interception does not corrupt runtime input state.
