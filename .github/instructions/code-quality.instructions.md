---
description: "Use when writing, reviewing, or generating code in any language. Enforces language-agnostic code quality: naming, error handling, resource management, algorithm choice, type safety, and anti-pattern avoidance."
applyTo: "**"
---

# Language-Agnostic Code Quality Rules

## Anti-patterns — never do these

- **Built-in shadowing**: Do not reassign or shadow built-in / reserved names
  (`list`, `type`, `map`, `filter`, `Array`, `Object`, `string`, `error`, etc.).
- **Silent failure**: Do not use bare `except: pass`, `catch {}`, or `if err != nil { _ = err }` without an explicit comment explaining the deliberate discard.
- **Resource leaks**: Do not open files, sockets, or connections without a guaranteed cleanup path on both success and error exits.
- **Blocking I/O on the main thread**: Do not issue synchronous network requests (e.g., sync `XMLHttpRequest`, Python `requests` on an asyncio loop, blocking DB calls in a UI thread).
- **Deprecated APIs**: Do not use APIs the language/runtime has deprecated — check the standard library first.
- **Reinventing standard library**: Do not reimplement sort, search, hash, or serialization when a standard library function exists.
- **Mutable when constant**: Do not declare a variable as mutable (`var`, `let`, unqualified declarations) when the value never changes after initialization.

## Required practices

- **Idiomatic resource management**: Use `with` (Python), `try-with-resources` (Java/Kotlin), `using` (C#), `defer` (Go), RAII/smart-pointers (C++).
- **Specific exceptions**: Catch the narrowest exception/error type possible; include the operation and relevant values in the message.
- **Type information**: Include type hints, annotations, or generics where the language supports them; do not use `any` / `object` / `interface{}` unless truly required.
- **Complexity annotation**: When using a non-trivial algorithm, annotate its time and space complexity with a short inline comment.
- **Immutable by default**: Prefer `const` / `final` / `val` / `readonly` bindings unless mutation is explicitly needed.

## Naming

- Descriptive names for all identifiers; single-letter names are only acceptable as loop indices (`i`, `j`, `k`).
- Functions should have verb names (`calculate`, `fetch`, `validate`); Boolean variables should read as predicates (`isReady`, `hasError`).
- Do not abbreviate unless the abbreviation is universally understood in the domain (e.g., `ctx`, `req`, `res`, `url`).

## On uncertainty

- If you are not certain an API or function exists, state that explicitly rather than guessing.
- If behavior is version-dependent, declare the version assumption inline.
- Prefer documented, idiomatic patterns over bespoke implementations.
