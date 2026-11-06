# Historical Academic Coursework Archive — IT305

**Original Project:** IT305 Computer Networks Socket Programming Project  
**Student Groups:** Groups 056 & 069 (PG28)  
**Historical Date:** October 2026  
**Implementation Language:** C++17 with POSIX Sockets & `pthreads`  
**Reference Dataset:** Animals-10 (Kaggle dataset with 10 topic subdirectories)

---

## Purpose of this Directory

This directory (`legacy/academic/cn-project/`) preserves the intact, unmodified codebase and experiment reports developed during the IT305 Computer Networks course.

These files serve as the historical foundation and proof-of-concept for the **Dibyt** file distribution product. They are archived here for academic history, provenance tracking, and retrospective benchmarking.

> **CRITICAL ARCHIVAL NOTICE:**
> 1. The code in this directory is preserved verbatim. It is **not** to be modernized, refactored, or linked into the production CMake build system of Dibyt.
> 2. The Makefiles in these subdirectories target standard Linux environments with `g++ -std=c++17 -pthread`.
> 3. These implementations are POSIX/Linux-specific and are not intended to be compiled on Windows without POSIX compatibility layers.

---

## Directory Inventory

| Path | Description | Key Demonstrated Mechanics |
|---|---|---|
| `README.md` | Original submission README | Full setup, execution parameters, and original run commands. |
| `FinalReport_Group_056_069.pdf` | Final project report | Coursework report describing design, methodology, and performance. |
| `Part1/` | Basic topic-based distribution | Sequential server vs. multi-threaded worker server. |
| `Part2_Case1/` | Fault injection without sessions | Probabilistic connection drop ($P=0.01\dots0.20$); client restarts from byte 0. |
| `Part2_Case2/` | Session management & resume | Client-side atomic checkpointing; byte-offset resumption via `lseek()`. |
| `Part2_Case2_Enhanced/`| Parallel TCP partitions | Multi-connection transfers with deterministic `file_index % workers` partitioning. |
| `Results/` | Experimental charts and data | Matplotlib plots and CSV data across Animals-10 test runs. |

---

## Technical Audit & Identified Limitations

The original coursework proved the viability of socket-level resumption and parallel streams, but exhibited key limitations that motivated the engineering of the Dibyt product:

1. **In-Memory Server State:**
   - In `Part2_Case2` and `Part2_Case2_Enhanced`, server session state was stored in process RAM (`std::map<std::string, Session>`). If the server crashed or restarted, active client sessions were lost.
   - *Dibyt Solution:* SQLite-backed transactional state store (Sprint 08) surviving complete server restarts.
2. **Imbalanced Modulo Partitioning:**
   - Files were statically partitioned via `worker_id = file_index % total_workers`. On uneven file size distributions, workers became severely imbalanced.
   - *Dibyt Solution:* Centralized dynamic leased task queue with Longest Processing Time (LPT) scheduling (Sprint 10).
3. **No Cryptographic Integrity Verification:**
   - Transfers trusted raw TCP streams without end-to-end file checksum verification.
   - *Dibyt Solution:* Mandatory OpenSSL EVP SHA-256 verification and quarantine staging before atomic finalization (Sprint 04, 07).
4. **Filesystem Boundary Security:**
   - Security relied on a string-filtering `safe_name()` helper.
   - *Dibyt Solution:* Strict canonical relative path isolation and symlink containment within designated destination roots (Sprint 04, 15).

---

## Historical Build Instructions (For Linux/POSIX environments)

To build any historical stage independently using its original Makefile:
```bash
# Example for Part2_Case2:
cd legacy/academic/cn-project/Part2_Case2
make clean && make
./server <dataset_dir> <port> [failure_prob]
./client <server_ip> <port> <topic> <output_dir>
```
