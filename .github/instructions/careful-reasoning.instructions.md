---
description: "Use when solving multi-step problems, implementing algorithms, answering questions about APIs or language features, performing code review, or any task where accuracy, correctness, and avoiding hallucination are critical."
---

# Careful Reasoning Guidelines

These rules compensate for the tendency of smaller or less capable models to assert uncertain facts
with false confidence, skip edge-case analysis, and hallucinate API signatures.

## Before writing any code

1. **Restate the problem** in your own words to confirm the goal is correctly understood.
2. **Identify constraints and edge cases**: empty inputs, null/nil/None, overflow, concurrency, network failure.
3. **Choose an algorithm or pattern explicitly**: name it, explain why it fits, and state its time and space complexity — O(?) notation inline.
4. **Verify API existence**: if you are not certain a function or module exists in the target language/version, say so. Do not fabricate a signature.

## While implementing

- Test your logic mentally against at least one normal case and one edge case before finalizing.
- If you discover an assumption mid-implementation, surface it immediately with a comment.
- Prefer standard-library implementations over custom ones — they are tested, documented, and maintained.
- Do not optimize prematurely. Write correct code first; add complexity only when a profiled bottleneck justifies it.

## After implementing

- List all assumptions made about inputs, environment, runtime version, or external behavior.
- Flag any code that relies on behavior not guaranteed by the language specification (implementation-defined behavior, platform-specific quirks).
- If the solution is a best-effort guess rather than a verified implementation, say so explicitly.

## Hallucination prevention checklist

- [ ] Every function, method, and module referenced actually exists in the stated version.
- [ ] No example output was invented; any shown output reflects real execution or is clearly labeled "expected output".
- [ ] Version-specific features state the minimum version they require (e.g., `Python ≥ 3.10`, `Node ≥ 18`).
- [ ] External library names and their APIs are cross-checked against known documentation, not guessed.

## Complexity communication

When the problem involves a non-trivial algorithm, always state:
- **Time complexity**: best case / average / worst case.
- **Space complexity**: auxiliary space used beyond the input.
- **Trade-offs**: what is sacrificed (memory, simplicity, generality) in exchange for better performance.

Example annotation:
```python
# Merge sort — O(n log n) time, O(n) auxiliary space.
# Chosen over quicksort for stable sort guarantee on linked structures.
```
