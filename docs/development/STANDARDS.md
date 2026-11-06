# Development Standards & Guidelines — Dibyt

**Document Status:** Approved Engineering Guidelines (v0.1)

---

## 1. Core Principles

Dibyt code is written for reliability, performance, and long-term maintainability. Code must read like production-grade systems software.

1. **Safety First:** Memory corruption, unbounded allocations, or race conditions are treated as critical bugs.
2. **Correctness Before Speed:** A transfer that completes in 2 seconds with corrupted bytes is a failure; a transfer that completes in 3 seconds with cryptographically verified bytes is a success.
3. **No Unbounded Primitives:** Every queue, thread pool, buffer, timeout, and retry attempt must have an explicit, documented upper bound.
4. **Explicit Ownership (RAII):** All system resources—sockets, file descriptors, SQLite handles, OpenSSL EVP contexts, and memory allocations—must be managed via RAII wrappers (`std::unique_ptr`, custom RAII structs).
5. **Separation of Concerns:** Keep network framing, domain transfer logic, storage transactions, and user interfaces strictly segregated.

---

## 2. Language Standard & Compiler Targets

- **Standard:** Modern C++20 (`-std=c++20`).
- **Compilers:**
  - GCC 11+
  - Clang 14+
  - MSVC 2022 (v143+) for portability checks
- **Core Standard Library Usage:**
  - Prefer `std::span` over raw pointers and length pairs.
  - Prefer `std::filesystem::path` for all path manipulations.
  - Prefer `std::expected<T, E>` (or equivalent robust error wrapper) over exceptions across library boundaries.
  - Use `std::chrono` for all timeouts, intervals, and timestamps.
  - Const-correctness is strictly enforced across all classes and functions.

---

## 3. Error Handling Taxonomy

Dibyt avoids throwing exceptions across subsystem boundaries. All operations that can encounter operational failures return an explicit result type:

```cpp
template <typename T>
using Result = std::expected<T, DibytError>;
```

### Error Taxonomy:
- **`DibytError::NetworkError`:** Transient socket errors, drops, EOF, or timeout. Usually retryable.
- **`DibytError::ProtocolViolation`:** Corrupted framing, invalid magic, or oversized payload. Fatal; aborts connection.
- **`DibytError::IntegrityMismatch`:** Computed SHA-256 does not match manifest. Triggers task retry.
- **`DibytError::StorageError`:** SQLite write failure, disk full, or permission error. Fatal to task or session.
- **`DibytError::SecurityViolation`:** Directory traversal attempt, unauthorized session token. Fatal; logged to audit log.

---

## 4. Code Formatting & Commenting Rules

### Formatting:
- 4-space indentation; no tabs.
- 100-character line width guideline.
- Clang-format configuration provided in `.clang-format`.

### Commenting Standards (Senior Engineer Tone):
- **Do not write redundant comments:**
  ```cpp
  // Bad: Increment counter by 1
  counter++;
  ```
- **Explain non-obvious invariants, rationale, and concurrency contracts:**
  ```cpp
  // Invariant: The worker lease must be held before calling commit_checkpoint().
  // Flushes receiver OS cache to disk before committing to SQLite to ensure that
  // any post-commit crash will find actual bytes persisted on storage.
  ```
- Document thread-safety expectations for every class in its header docstring.

---

## 5. Branch & Commit Policy

1. All development work occurs directly on `main` for early stages as directed.
2. Commits must be cohesive and describe the exact technical change.
3. **Commit messages must never mention AI, agents, or tools.**
4. Changes must always leave the repository in a clean, compiling, and passing state.
