# Historical Coursework Prototype Analysis

**Subject:** IT305 Socket Programming Coursework (`CN_Project`)  
**Historical Groups:** 056 & 069 (PG28)  
**Archive:** `CN_Project.zip`  
**Document Status:** Retrospective Analysis & Provenance Audit

---

## 1. Overview of Academic Artifacts

The academic predecessor project implemented a topic-based file distribution service using low-level POSIX TCP sockets in C++17. The project evolved across four progressive stages, culminating in benchmark experiments against the Animals-10 dataset (10 image categories).

```text
CN_Project/
├── Part1/                  # Baseline sequential vs multi-threaded distribution
├── Part2_Case1/            # Fault injection without session management (restart from 0)
├── Part2_Case2/            # Session management and checkpoint-based resume
├── Part2_Case2_Enhanced/   # Parallel TCP connections with modulo file partitioning
├── Results/                # Historical benchmark CSVs and matplotlib charts
└── FinalReport_Group_056_069.pdf # Coursework submission report
```

---

## 2. Technical Audit by Stage

### Stage 1: Part 1 (`Part1/`)
- **Demonstrated Behavior:** Implemented topic directory mapping on the server. Clients sent a plain-text ASCII command `GET <topic>\n`. The server responded with file counts and transferred files sequentially using length-framed messages (`uint32_t` filename length, filename string, `uint64_t` file size, raw binary bytes). Compared single-threaded handling with `pthread_create`-based concurrent server threads.
- **Observed Code Characteristics:**
  - Used `send_all()` and `recv_exact()` utility loops to handle partial TCP frames and `EINTR`.
  - Buffer chunks fixed at 64 KiB (`65536` bytes).
  - Used `safe_name()` string check to filter paths containing `/`, `\`, `.`, or `..`.
- **Key Limitations:** No fault tolerance; any socket drop terminated the transfer permanently.

### Stage 2: Part 2 Case 1 (`Part2_Case1/`)
- **Demonstrated Behavior:** Introduced client-side probabilistic fault injection. A random generator artificially closed client socket connections with probability $P_{\text{fail}} \in [0.01, 0.20]$. When disconnected, the client re-established a new TCP connection and re-requested the entire topic from byte zero.
- **Observed Code Characteristics:**
  - Simulates network drops cleanly in client receive loops.
  - Demonstrated that high failure rates drastically inflate transfer time and waste network bandwidth due to repeated transfers of byte ranges.

### Stage 3: Part 2 Case 2 (`Part2_Case2/`)
- **Demonstrated Behavior:** Introduced server-side session tracking and client-side checkpointing.
  - Server tracked client session IDs and current file byte offsets in memory.
  - Client periodically saved received byte counts to a checkpoint file on disk (using write to temporary file followed by rename for crash safety).
  - Checkpoint interval defaulted to 1 MiB (`1048576` bytes).
  - Upon reconnection, client supplied session ID and confirmed offset; server used `lseek()` to resume streaming without re-sending earlier bytes.
- **Key Limitations:**
  - **In-Memory Server State:** Server sessions were stored in a `std::map<std::string, Session>` in process RAM. If the server process crashed or restarted, all active client sessions were destroyed, forcing clients to restart from scratch.
  - Client state was tracked in plain-text flat files rather than a transactional database.

### Stage 4: Part 2 Case 2 Enhanced (`Part2_Case2_Enhanced/`)
- **Demonstrated Behavior:** Established multiple parallel TCP sockets per transfer (defaulting to 3 workers, configurable from 1 to 32).
  - Partitioned files across connections using deterministic modulo arithmetic: `worker_id = file_index % total_workers`.
  - Each worker managed independent session resumption and socket error handling.
- **Key Limitations:**
  - **Load Imbalance:** Modulo partitioning assumes files are homogeneous in size. If a topic contains one 500 MB file and ten 10 KB files, worker 0 is saturated for minutes while workers 1 and 2 finish in milliseconds and sit idle.
  - **Coupled Worker State:** Workers operated with static file assignment; no dynamic work-stealing or re-leasing existed if a single worker suffered unrecoverable connection failure.

---

## 3. Comparative Matrix: Academic Prototype vs. Production Dibyt

| Dimension | Academic Prototype (`CN_Project`) | Dibyt Product Target (v0.1) |
|---|---|---|
| **Language Standard** | C++17 | Modern C++20 (`std::expected`, concepts, ranges) |
| **Build System** | Static manual `Makefile`s | Modular CMake 3.22+ with presets and target export |
| **Integrity Verification** | None (trusted raw bytes over TCP) | Cryptographic OpenSSL EVP SHA-256 for all files |
| **Persistence Engine** | In-memory `std::map` (server) & text files (client) | Transactional SQLite with schema migrations |
| **Crash Durability** | Server crash loses all session state | Full restart recovery: persistent leases & checkpoint tables |
| **Work Scheduling** | Static modulo (`file_index % workers`) | Dynamic leased task queue with heartbeat & work-stealing |
| **Path & FS Security** | Basic `safe_name()` string check | Canonical relative path resolution, symlink jail enforcement |
| **Framing & Protocol** | Newline ASCII headers + raw binary | Explicit binary protocol header, magic bytes, length limits |
| **Staging & Finalization**| Writes directly into output directory | Staged in quarantined temp dir; atomic rename on SHA match |
| **Concurrency Limits** | Unbounded thread spawn per connection | Bounded thread pool, backpressure, connection caps |
| **Testing** | Manual bash scripts & Kaggle dataset | Automated Unit, Integration, and Fault-Injection tests |

---

## 4. Preservation & Archival Strategy

In accordance with `ANTIGRAVITY_GLOBAL_RULES.md`:
1. The academic coursework files, PDF report, and benchmark assets will be preserved intact under `legacy/academic/cn-project/` during Sprint 02.
2. No legacy code will be altered, modernized, or refactored in place.
3. Historical benchmark figures (from `Results/`) are documented as academic coursework observations under specific synthetic conditions, not as certified product performance benchmarks.
