# Coding Standards — Workspace Baseline

These rules apply to every task in this workspace.
They are designed to compensate for weaknesses common in smaller or less capable models:
poor code quality, silent failures, hallucinated APIs, and unsafe patterns.

## Naming and scope

- Never shadow or reassign built-in / reserved names (`list`, `type`, `map`, `Array`, `Object`, `string`, etc.).
- Use descriptive names. Single-letter identifiers are acceptable only for loop indices (`i`, `j`, `k`).
- Prefer `const` / `final` / `val` / `readonly` over mutable bindings when the value never changes.

## Algorithm and data structure choice

- Prefer standard-library sort, search, and data-structure implementations over hand-rolled alternatives.
- When you choose a non-trivial algorithm, state its time and space complexity inline.
- Do not reimplement what the standard library already provides.

## Resource management

- Always use the language's idiomatic resource-management construct:
  - Python → `with`
  - Java / Kotlin → `try-with-resources`
  - C# → `using`
  - Go → `defer`
  - C++ → RAII / smart pointers
- File handles, sockets, database connections, and locks must be released on all exit paths — success and error.

## Asynchronous and I/O patterns

- Prefer async / non-blocking I/O wherever the language and runtime support it.
- Never issue synchronous network calls that block the main thread or event loop.
- Do not use deprecated I/O APIs (e.g., synchronous `XMLHttpRequest`, bare `threading.Thread` without lifecycle management).

## Error handling

- Never swallow errors silently. A bare `except: pass`, `catch {}`, or `_ = err` is always wrong unless explicitly justified with a comment.
- Always catch the most specific exception / error type available.
- Include actionable context in error messages: what operation failed, with what values, and why.
- Validate inputs at system boundaries (public functions, API handlers, CLI entry points). Do not re-validate deep inside pure logic.

## Uncertainty

- If you are not certain that a function, module, or API exists, say so explicitly — do not fabricate a signature.
- If behavior is version-dependent, state the version assumption.
- Prefer well-documented idioms over custom implementations when both achieve the same goal.
