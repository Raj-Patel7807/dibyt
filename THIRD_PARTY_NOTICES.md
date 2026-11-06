# Third-Party Notices & Attribution — Dibyt

This document records third-party libraries, toolchains, specifications, and historical artifacts referenced or incorporated into Dibyt.

---

## 1. Planned Direct Dependencies (v0.1 Developer Preview)

The following open-source dependencies are planned for integration in subsequent implementation sprints:

| Component | Target Milestone | Purpose | License | Source / Upstream |
|---|---|---|---|---|
| **SQLite3** | Sprint 08 | Embedded metadata, task leases, and durable checkpoint storage | Public Domain / SQLite Blessing | https://www.sqlite.org |
| **OpenSSL (libcrypto)** | Sprint 04, 07, 15 | EVP SHA-256 streaming digests and TLS encryption | Apache-2.0 | https://www.openssl.org |
| **CLI11** | Sprint 13 | Header-only modern C++ command line parser | BSD 3-Clause | https://github.com/CLIUtils/CLI11 |
| **Catch2** or **GoogleTest** | Sprint 03 | Unit and integration test framework | BSL-1.0 / BSD 3-Clause | https://github.com/catchorg/Catch2 |

*Note: Formal compliance notices and full license texts for these libraries will be imported when their build-time integration occurs in Sprint 03 through Sprint 08.*

---

## 2. Historical Academic Artifacts

- **Origin:** IT305 Computer Networks Socket Programming Project (Groups 056 & 069, PG28).
- **Scope:** Files located under `legacy/academic/cn-project/` (scheduled for migration in Sprint 02).
- **Attribution Notice:** Coursework code and submission report authored jointly by group members. These materials are preserved for academic history and provenance. See [`docs/LEGAL_AND_RIGHTS.md`](docs/LEGAL_AND_RIGHTS.md).
