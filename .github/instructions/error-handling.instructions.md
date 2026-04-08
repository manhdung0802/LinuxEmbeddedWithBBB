---
description: "Use when implementing error handling, exception management, retry logic, fallback strategies, defensive programming, or any code path that deals with failures in any language."
---

# Error Handling Rules

## Core principle

Every error path must either:
1. **Recover** — handle the error meaningfully and continue with a safe state, or
2. **Propagate** — re-throw/return the error with added context, or
3. **Terminate** — fail fast with a clear, actionable message.

Silently discarding an error is never acceptable without an explicit, documented reason.

## Wrapping external calls

Wrap all external operations (file I/O, network, database, sub-process, third-party SDK) in explicit error handling:
- Use try/catch, Result/Either types, or error-return conventions as appropriate to the language.
- Include in the error message: what operation failed, the relevant input values, and the underlying cause.

## Language-agnostic checklist

Before considering any error-handling implementation complete, verify:
- [ ] No bare `catch {}` / `except: pass` / `if err != nil { _ = err }` without an explanatory comment.
- [ ] Resources (files, connections, locks) are freed on both success and error paths.
- [ ] Retries — if applicable — use exponential backoff with a capped maximum attempt count.
- [ ] User-facing messages do not expose internal stack traces, file paths, or sensitive data.
- [ ] Errors are logged or surfaced at the appropriate severity level (debug / info / warn / error / fatal).
- [ ] The narrowest exception / error type available is caught, not the base class.

## Specific anti-patterns to avoid

| Anti-pattern | Why it is wrong |
|---|---|
| `except Exception: pass` | Hides all errors including bugs, KeyboardInterrupt, SystemExit. |
| `catch (Exception e) {}` | Swallows the error; caller and operator see nothing. |
| Returning `null`/`None`/`undefined` on error | Forces every caller to check for null with no context about why it failed. |
| Catching broad type to check message string | Fragile; error text is not a stable API surface. |
| Retrying without backoff | Can flood downstream services and worsen cascading failures. |

## Result / Option types (preferred where idiomatic)

In languages that support them (`Result<T,E>` in Rust, `Either` in Haskell/Scala/Arrow, `std::expected` in C++23), prefer returning typed error values over throw/catch for expected failure conditions.

## Error message template

```
[Operation] failed [during what phase]: [error detail]. Input was: [relevant values].
```

Example: `"File read failed during config load: Permission denied. Path: /etc/app/config.yaml"`
