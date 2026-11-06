# ADR-0004: Linux-First Platform Support with Portability Stubs

**Status:** Proposed  
**Date:** 2026-11-06  
**Deciders:** Raj Patel (Project Maintainer)  

---

## Context and Problem Statement
The predecessor academic project was authored strictly for POSIX/Linux platforms using native Linux socket syscalls (`<sys/socket.h>`, `<netinet/in.h>`, `send(2)`, `recv(2)`). While cross-platform support (Windows/macOS) is valuable, optimizing for multiple OS network stacks simultaneously during early foundation sprints introduces premature complexity.

---

## Decision Drivers
- High development velocity for foundational sprints.
- Native access to high-performance Linux socket features (`epoll`, `splice`, `fdatasync`).
- Honest representation of tested environments: no false claims of Windows/macOS support without rigorous CI testing.

---

## Considered Options
1. **Immediate Full Cross-Platform Abstraction (Boost.Asio / libuv):** Introducing heavyweight cross-platform networking frameworks upfront.
2. **Linux-First Native POSIX with Portability Abstraction Layer (Chosen):** Authoring clean socket wrapper interfaces in `libs/networking`, targeting Linux initially, while keeping standard C++20 for all domain, storage, and protocol logic.

---

## Decision Outcome
Chosen option: **Linux-First Native POSIX with Portability Stubs**.

### Positive Consequences
- **Direct Control:** Zero overhead socket management without complex runtime event engine dependencies.
- **Honest Platform Boundaries:** Clearly documents that Linux is the certified deployment target for v0.1.
- **Portability Pathway:** Modern C++ standard library types (`std::filesystem`, `std::chrono`, `std::jthread`) ensure non-networking modules compile cleanly on any modern C++20 platform.

### Negative Consequences / Trade-offs
- Native Windows support is deferred to later sprints (requiring Winsock2 abstraction).
