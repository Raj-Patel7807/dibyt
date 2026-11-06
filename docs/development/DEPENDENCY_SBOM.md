# Software Bill of Materials & Dependency License Inventory — Dibyt

**Release:** Dibyt v0.1.0-dev  
**Date:** 2026-11-06  
**License:** Apache-2.0  

---

## 1. Native C++ Core Components (`libs/` and `apps/`)

All core C++ libraries (`libs/core`, `libs/manifest`, `libs/protocol`, `libs/networking`, `libs/storage`, `libs/transfer`, `libs/scheduler`, `libs/observability`, `libs/security`, `apps/dibyt-server`, `apps/dibyt-cli`, `apps/dibyt-api`) are implemented directly against standard C++20 with **zero runtime external dependencies**.

| Component | License | Origin | Linkage |
| :--- | :--- | :--- | :--- |
| **ISO C++ Standard Library (libc++ / libstdc++)** | Apache 2.0 with LLVM Exception / GPL-3.0 with GCC Runtime Exception | GCC / Clang | Static / System Dynamic |
| **Winsock2 (`ws2_32.lib`)** *(Windows Only)* | Microsoft OS API | Microsoft Windows SDK | System Dynamic |
| **POSIX Sockets & Pthreads** *(Linux Only)* | GNU LGPL / glibc | System C Library | System Dynamic |

---

## 2. Web Console Components (`apps/dibyt-dashboard/`)

The web management console is an optional client-side single-page application built with React and Vite.

| Dependency | Version Range | License | Purpose |
| :--- | :--- | :--- | :--- |
| **react** | `^19.0.0` | MIT | Declarative UI framework |
| **react-dom** | `^19.0.0` | MIT | DOM rendering bindings |
| **lucide-react** | `^0.460.0` | MIT | Iconography |
| **typescript** | `^5.6.3` | Apache-2.0 | Compile-time static type checking (dev) |
| **vite** | `^6.0.1` | MIT | Frontend asset bundler (dev) |
| **@vitejs/plugin-react** | `^4.3.4` | MIT | Fast refresh integration (dev) |

---

## 3. Historical Academic Coursework (`legacy/academic/cn-project/`)

The historical prototype is maintained for academic provenance and benchmarking comparison.

- **Status:** Archived legacy reference code.
- **Licensing Notice:** Authored by Group 056 / 069 (Course Project, 2026).
- **Scope:** Kept isolated in `legacy/academic/cn-project/`; not linked or included in production binaries or container images.
