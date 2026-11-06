# Dependency Policy & Management — Dibyt

**Document Status:** Approved Dependency Guidelines (Sprint 03)  
**Tooling Strategy:** Pinned Source / CMake FetchContent / System Packages

---

## 1. Principles for External Dependencies

Dibyt follows a disciplined dependency policy to maintain reproducibility, security, and offline build capability:

1. **Minimal Surface Area:** Every dependency introduces supply-chain risk and maintenance burden. Avoid dependencies where standard C++20 standard library facilities suffice.
2. **Pinned Versions Only:** Unpinned `master` / `main` branches are strictly prohibited. All external dependencies must specify an exact release tag or immutable commit SHA.
3. **No Hidden Network Fetches in Air-Gapped Environments:** Where practical, dependencies can be populated via vendored submodules or system package managers (`apt`, `vcpkg`, `conan`).
4. **License Compliance:** Every dependency must be permissively licensed (MIT, Apache-2.0, BSD, BSL-1.0, or Public Domain). Copyleft licenses (GPL/AGPL) that compromise the Apache-2.0 licensing model are disallowed for Dibyt product code.

---

## 2. Staged Dependency Roadmap

Dependencies are integrated on a strictly just-in-time basis as their specific sprint prompts arrive:

| Component | Target Sprint | Primary Capability | Intended Integration |
|---|---|---|---|
| **Catch2** (v3.5+) / **GTest** | Sprint 03 / 04 | Unit & Integration Test Runner | CMake `FetchContent` (Pinned tag) |
| **OpenSSL (libcrypto)** | Sprint 04, 07, 15 | EVP SHA-256 Streaming Digests & TLS | `find_package(OpenSSL REQUIRED)` (System pkg) |
| **SQLite3** | Sprint 08 | Transactional State & Checkpoint Database | Amalgamation or `find_package(SQLite3 REQUIRED)` |
| **CLI11** | Sprint 13 | Production Command-Line Interface Parsing | Single header / CMake `FetchContent` |

---

## 3. Dependency Verification Checklist
Before introducing a new external dependency in any sprint:
1. Verify license compatibility and add attribution to [`THIRD_PARTY_NOTICES.md`](../../THIRD_PARTY_NOTICES.md).
2. Pin the exact release commit SHA.
3. Ensure the dependency does not link against legacy coursework code.
