---
name: rapid-architecture-iteration
description: Rebuild an explicitly scoped subsystem around its target architecture without maintaining legacy API compatibility. Use only when the user explicitly requests rapid iteration, a clean break, or direct replacement of old interfaces.
---

# Rapid Architecture Iteration

## Activation

Apply only after an explicit user instruction such as “快速迭代模式”, “不用兼容旧接口”, “直接按新设计重构”, or an equivalent clean-break request. The user must identify the subsystem or approve the scope.

## Working rules

- Treat the target architecture as the source of truth. Delete or replace obsolete APIs instead of adding adapters, dual paths, fallback behavior, or deprecation wrappers.
- Make a short dependency inventory before editing, then change the owning data model and public contract first. Update consumers in the same change.
- Preserve only contracts the user explicitly retains: serialized/project data, external integrations, or named tests. Do not preserve internal legacy call sites by default.
- Remove obsolete build targets, source files, bindings, and tests once their replacements are verified. Do not leave dead compatibility code.
- Keep platform/RHI boundaries intact. A clean break does not authorize backend-specific leaks into business code.
- Keep destructive scope narrow. Never delete user assets, project content, generated work, or unrelated changes without explicit approval.

## Verification

- Build the affected targets.
- Run tests that validate the new contract; replace tests that only assert removed legacy behavior.
- Run source-boundary checks and inspect the diff for stale references to deleted interfaces.
- Report the broken compatibility surface, deleted interfaces, and any deliberate migration requirement.

## Default delivery style

Lead with what was replaced and the new contract. State clearly that this was a deliberate non-compatible change, then list validation results and remaining migration work.
