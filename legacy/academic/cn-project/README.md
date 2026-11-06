# IT305 Socket Programming Project — Groups 056 & 069
## Fault-Tolerant Topic-Based File Distribution Service

**Course:** IT305: Socket Programming Project  
**Lab Groups:** Groups 056 & 069 (PG28)  
**Implementation Language:** C++17 with POSIX Sockets & `pthreads`  
**Reference Dataset:** Animals-10 (10 Topic Subdirectories)  

---

## Table of Contents
1. [Project Overview & Architectural Design](#1-project-overview--architectural-design)
2. [Dataset and Topic Layout](#2-dataset-and-topic-layout)
3. [Repository Directory Structure](#3-repository-directory-structure)
4. [Build Instructions](#4-build-instructions)
5. [Detailed Execution & Run Instructions](#5-detailed-execution--run-instructions)
   - [Part 1: Basic Topic-Based Distribution](#part-1-basic-topic-based-distribution)
   - [Part 2 Case 1: No Session Management Baseline](#part-2-case-1-no-session-management-baseline)
   - [Part 2 Case 2: Session Management & Checkpoint Resume](#part-2-case-2-session-management--checkpoint-resume)
   - [Part 2 Case 2 Enhanced: Parallel TCP Partitions](#part-2-case-2-enhanced-parallel-tcp-partitions)
6. [Fault-Injection Mechanism](#6-fault-injection-mechanism)
7. [Session Management, Checkpointing, and Fault Recovery](#7-session-management-checkpointing-and-fault-recovery)
8. [Performance Benchmarking and Results Directory](#8-performance-benchmarking-and-results-directory)
9. [Troubleshooting Guide](#9-troubleshooting-guide)
10. [Design Document Compliance & Documented Deviations](#10-design-document-compliance--documented-deviations)

---

## 1. Project Overview & Architectural Design

This project implements a high-performance, fault-tolerant topic-based file distribution service using low-level POSIX TCP sockets in C++17. The system follows a client-server architecture where the server exposes image categories (topics) mapped to filesystem directories, and clients request a topic to download all associated files.

The project is structured into four distinct, self-contained implementations:

1. **Part 1 (`Part1/`):** Baseline topic distribution service comparing **single-threaded sequential** client handling with a **multi-threaded concurrent worker** model. Provides length-framed binary transfer without injected failures.
2. **Part 2 Case 1 (`Part2_Case1/`):** Multi-threaded server with probabilistic connection interruptions (fault injection) and a baseline **no-session-management** recovery model. Interrupted transfers discard all progress and restart from the beginning.
3. **Part 2 Case 2 (`Part2_Case2/`):** Multi-threaded server featuring **stateful session tracking** and **checkpoint-based resume**. Periodically commits byte offsets to disk; clients resume interrupted file streams at the exact validated byte offset without retransmitting already delivered data.
4. **Part 2 Case 2 Enhanced (`Part2_Case2_Enhanced/`):** High-throughput extension establishing **multiple simultaneous TCP connections** per topic transfer. Files are partitioned among parallel connections, each maintaining independent session tracking and failure recovery.

### Wire Transmission & Framing Safety
Across all implementations, network operations use dedicated transmission loops (`send_all()` and `recv_exact()`) to protect against partial TCP socket reads and writes and handle interrupted system calls (`EINTR`). Control headers are newline-terminated ASCII lines, while file names and binary payloads are length-framed:
- Control messages: Newline-terminated ASCII lines (e.g., `GET <topic>\n`, `OK <topic> <count>\n`).
- Binary safety: File sizes and lengths are explicitly declared so binary bytes are never confused with protocol delimiters.
- Directory traversal security: The `safe_name()` utility validates that all requested topics and filenames contain no directory separators (`/`, `\`) or traversal elements (`.`, `..`).

---

## 2. Dataset and Topic Layout

The service distributes the **Animals-10** image dataset ([Kaggle dataset](https://www.kaggle.com/datasets/alessiocorrado99/animals10)). 

### Server-Side Dataset Hierarchy
The server dynamically maps directory names to topic keys. The dataset root directory provided via the CLI must contain one subdirectory per topic:
```text
/path/to/raw-img/
├── cane/         (dog images)
├── cavallo/      (horse images)
├── elefante/     (elephant images)
├── farfalla/     (butterfly images)
├── gallina/      (chicken images)
├── gatto/        (cat images)
├── mucca/        (cow images)
├── pecora/       (sheep images)
├── ragno/        (spider images)
└── scoiattolo/   (squirrel images)
```
*Note: Absolute dataset paths are never hard-coded in the source code; the directory is loaded dynamically at server launch.*

### Client-Side Output Layout
Clients write received files to `output_dir/topic/`:
```text
output_dir/
└── cane/
    ├── OIP-1.jpeg
    ├── OIP-2.jpeg
    └── ...
```

---

## 3. Repository Directory Structure

```text
FinalSubmission_Group_056_069/
├── README.md                      # This comprehensive documentation file
├── FinalReport_Group_056_069.md   # Final report template (to be rendered to PDF)
├── Part1/                         # Part I: Basic topic distribution
│   ├── common.hpp                 # Network socket and string helpers
│   ├── server.cpp                 # Single-threaded baseline & multi-threaded server
│   ├── client.cpp                 # Topic retrieval client
│   └── Makefile                   # Builds server and client
├── Part2_Case1/                   # Part II Case 1: Restart from beginning
│   ├── common.hpp                 # Shared network utilities
│   ├── server.cpp                 # Fault-injecting multi-threaded server
│   ├── client.cpp                 # Reconnecting restart-from-beginning client
│   └── Makefile                   # Builds server and client
├── Part2_Case2/                   # Part II Case 2: Session management & resume
│   ├── common.hpp                 # Shared network utilities
│   ├── server.cpp                 # Session-tracking & checkpoint-validating server
│   ├── client.cpp                 # Checkpointing client with random-access resume
│   └── Makefile                   # Builds server and client
├── Part2_Case2_Enhanced/          # Part II Case 2 Enhanced: Parallel connections
│   ├── common.hpp                 # Shared network utilities
│   ├── server.cpp                 # Partition-aware concurrent server
│   ├── client.cpp                 # Multi-connection parallel worker client
│   └── Makefile                   # Builds server and client
└── Results/                       # Performance evaluation scripts and logs
    ├── results_template.csv       # Starter CSV header for recorded measurements
    └── run_experiment.sh          # Concurrency benchmarking harness script
```

---

## 4. Build Instructions

### Prerequisites
- **Operating System:** Linux (Ubuntu 20.04/22.04/24.04 recommended), POSIX-compliant Unix, or Windows Subsystem for Linux (WSL).
- **Compiler:** `g++` supporting C++17 (GCC 9.0+ or Clang 10.0+).
- **Libraries:** POSIX Threads (`-pthread`), standard C++ filesystem library (`<filesystem>`).
- **Build Tool:** GNU `make`.

### Building Each Implementation

Each implementation is fully self-contained with its own source files and `Makefile`. Build each case inside its respective folder:

#### 1. Build Part 1
```sh
cd Part1
make clean
make
```
* **Targets:** `all` builds `server` and `client`.
* **Optional Target:** `make server_mt` compiles a dedicated multi-threaded binary (`-DMULTITHREADED_DEFAULT=1`).
* **Confirmation:** Run `ls -l server client` to verify executables exist.

#### 2. Build Part 2 Case 1
```sh
cd ../Part2_Case1
make clean
make
```
* **Targets:** `all` builds `server` and `client`.
* **Confirmation:** Run `ls -l server client` to verify executables exist.

#### 3. Build Part 2 Case 2
```sh
cd ../Part2_Case2
make clean
make
```
* **Targets:** `all` builds `server` and `client`.
* **Confirmation:** Run `ls -l server client` to verify executables exist.

#### 4. Build Part 2 Case 2 Enhanced
```sh
cd ../Part2_Case2_Enhanced
make clean
make
```
* **Targets:** `all` builds `server` and `client`.
* **Confirmation:** Run `ls -l server client` to verify executables exist.

---

## 5. Detailed Execution & Run Instructions

All implementations conform to the required command-line interfaces.

### Networking Notes (WSL & Multi-Node Setup)
- **Local Testing on Same Host:** Use `127.0.0.1` as the `server_ip`.
- **WSL to Windows / Cross-Machine Testing:** Find the server's LAN IP using `hostname -I` or `ip addr show eth0`. Ensure the selected port is open in your firewall.
- **Port Selection:** Use unprivileged ports between `1024` and `65535` (e.g., `5000`–`5003`).

---

### Part 1: Basic Topic-Based Distribution

#### Server Invocations
```sh
cd Part1
# 1. Single-threaded sequential baseline (default)
./server <port> <topics_root_dir>

# 2. Multi-threaded concurrent worker mode (via CLI flag)
./server <port> <topics_root_dir> --multithreaded

# 3. Multi-threaded mode via environment variable
IT305_MULTITHREADED=1 ./server <port> <topics_root_dir>
```
* **`<port>`:** Listening port number (e.g., `5000`).
* **`<topics_root_dir>`:** Absolute or relative path to dataset root containing topic subdirectories.

#### Client Invocation
```sh
./client <server_ip> <server_port> <topic> <output_dir>
```
* **Example:**
  ```sh
  ./client 127.0.0.1 5000 cane ./downloads
  ```

#### Actual Wire Protocol (Part 1)
```text
Client -> Server:  GET cane\n
Server -> Client:  OK cane 1513\n
Server -> Client:  FILE 28 32410\n
Server -> Client:  <28 filename bytes>
Server -> Client:  <32410 raw binary bytes>
... (repeated for all files in topic)
Server -> Client:  END\n
```

---

### Part 2 Case 1: No Session Management Baseline

Case 1 evaluates performance under probabilistic connection failures when **no session metadata** is preserved. Interrupted transfers wipe all partial files and restart from byte 0 of file 1.

#### Server Invocation
```sh
cd Part2_Case1
./server <port> <topics_root_dir> <failure_probability>
```
* **`<failure_probability>`:** Per-chunk failure probability from `0.0` (no faults) to `1.0` (always fail).
* **Example:**
  ```sh
  ./server 5001 /home/user/Animals10/raw-img 0.005
  ```

#### Client Invocation
```sh
./client <server_ip> <server_port> <topic> <output_dir>
```
* **Optional Environment Configurations:**
  - `IT305_MAX_RETRIES`: Maximum reconnect attempts before giving up (default: `50`).
  - `IT305_RETRY_DELAY_MS`: Base reconnect delay in milliseconds (default: `50`).
* **Example:**
  ```sh
  IT305_MAX_RETRIES=100 ./client 127.0.0.1 5001 cane ./downloads
  ```

---

### Part 2 Case 2: Session Management & Checkpoint Resume

Case 2 maintains state on both client and server so an interrupted connection resumes from the last acknowledged checkpoint rather than starting over.

#### Server Invocation
```sh
cd Part2_Case2
./server <port> <topics_root_dir> <failure_probability>
```
* **Optional Environment Configuration:**
  - `IT305_CHECKPOINT_INTERVAL`: Checkpoint interval in bytes (default: `1048576` = 1 MiB; automatically drops to `64 KB` for high failure rates $p \ge 0.10$).
* **Example:**
  ```sh
  ./server 5002 /home/user/Animals10/raw-img 0.01
  ```

#### Client Invocation
```sh
./client <server_ip> <server_port> <topic> <output_dir>
```
* **Optional Environment Configuration:**
  - `IT305_MAX_RECONNECTS`: Maximum reconnection attempts (default: `75`).
* **Example:**
  ```sh
  ./client 127.0.0.1 5002 cane ./downloads
  ```

#### Actual Wire Protocol (Part 2 Case 2)
```text
Initial Connection:
Client -> Server:  GET cane\n
Server -> Client:  SESSION S3a7b9e 1513 1048576\n
Server -> Client:  FILE 28 32410 0\n
Server -> Client:  <28 filename bytes>
Server -> Client:  DATA 0 32410\n + <32410 binary bytes>
Client -> Server:  CHECKPOINT S3a7b9e OIP-1.jpeg 32410\n
Server -> Client:  ACK S3a7b9e OIP-1.jpeg 32410\n
[Connection Drops mid-stream on next file]

Reconnection / Resume:
Client -> Server:  RESUME S3a7b9e cane OIP-2.jpeg 65536\n
Server -> Client:  RESUME_OK OIP-2.jpeg 65536 1048576\n
Server -> Client:  FILE 28 128000 65536\n
Server -> Client:  <28 filename bytes>
Server -> Client:  DATA 65536 62464\n + <62464 binary bytes>
Client -> Server:  CHECKPOINT S3a7b9e OIP-2.jpeg 128000\n
Server -> Client:  ACK S3a7b9e OIP-2.jpeg 128000\n
...
Server -> Client:  END\n
```

---

### Part 2 Case 2 Enhanced: Parallel TCP Partitions

Enhanced mode establishes **multiple concurrent TCP connections** for a single topic. Files are partitioned among connections using deterministic modulo arithmetic (`sorted_index % parts == part`).

#### Server Invocation
```sh
cd Part2_Case2_Enhanced
./server <port> <topics_root_dir> <failure_probability>
```
* **Example:**
  ```sh
  ./server 5003 /home/user/Animals10/raw-img 0.01
  ```

#### Client Invocation
```sh
# Standard 4-argument invocation (defaults to 3 parallel connections)
./client <server_ip> <server_port> <topic> <output_dir>

# Optional 5th argument: specify connection count (e.g., 4 partitions)
./client <server_ip> <server_port> <topic> <output_dir> [parallel_partitions]
```
* **`<parallel_partitions>`:** Optional number of parallel TCP worker threads (range: `1` to `32`, default: `3`).
* **Example:**
  ```sh
  # Default (3 parallel connections):
  ./client 127.0.0.1 5003 cane ./downloads

  # 4 parallel connections:
  ./client 127.0.0.1 5003 cane ./downloads 4
  ```

---

## 6. Fault-Injection Mechanism

Fault injection is implemented in `Part2_Case1`, `Part2_Case2`, and `Part2_Case2_Enhanced`. It tests the robustness of the recovery mechanisms under controlled conditions.

### 1. Where Decision Occurs
- The server evaluates the failure condition **immediately after transmitting each data chunk** (`CHUNK_SIZE = 64 KiB`).
- A 64-bit Mersenne Twister pseudo-random generator (`std::mt19937_64`) produces a uniform real value in $[0.0, 1.0)$.
- If `dist(rng) < failure_probability`, the server logs `FAULT INJECTED`, immediately closes the client socket without sending further bytes, and terminates the worker thread.

### 2. Meaning of Probability Values
The parameter represents a **per-chunk failure probability**, NOT a per-transfer probability:
- `0.0`: Injected faults disabled. Clean, uninterrupted file transfer.
- `0.001`: $0.1\%$ chance of socket closure after any 64 KB chunk.
- `0.01`: $1.0\%$ chance of socket closure after any 64 KB chunk.
- `0.10`: $10.0\%$ chance of socket closure after any 64 KB chunk.

### 3. Cumulative Effective Probability
A file of size $s$ transmitted in $c = 64\text{ KiB}$ chunks undergoes $n = \lceil s / c \rceil$ evaluation checks. The probability of experiencing at least one failure during that file transfer is:
$$P(\text{failure}) = 1 - (1 - p)^n$$

For a topic containing 1,500 images ($50\text{ MB}$ total, $\approx 800$ chunks):
- At $p = 0.001$: $1 - (0.999)^{800} \approx 55\%$ chance of at least one failure.
- At $p = 0.01$: $1 - (0.99)^{800} > 99.97\%$ (failure is virtually guaranteed).
- At $p = 0.05$: Failure occurs roughly every 20 chunks ($\approx 1.28\text{ MB}$).

*Note: Part 1 does not support fault injection.*

---

## 7. Session Management, Checkpointing, and Fault Recovery

| Feature | Part 1 | Part 2 Case 1 | Part 2 Case 2 | Part 2 Case 2 Enhanced |
| :--- | :--- | :--- | :--- | :--- |
| **Fault Handling** | Abort on error | Discard & restart | Checkpoint & resume | Parallel independent resume |
| **Session Tracking** | Stateless | Stateless | Server & client state | Independent per-partition state |
| **Checkpoint Storage** | None | None | `.<topic>.checkpoint` | `.<topic>.part<N>.checkpoint` |
| **Local File Handling** | Deletes partial file | Wipes topic directory | Seeks into partial file (`seekp`) | Seeks into partial partition file |
| **Retry Limit** | None (single attempt) | 50 attempts (`IT305_MAX_RETRIES`) | 75 reconnects (`IT305_MAX_RECONNECTS`) | 75 reconnects per worker |

### Detailed Recovery Workflows

#### Part 2 Case 1 Recovery (Restart Baseline)
1. Upon socket disconnection, `Part2_Case1/client.cpp` catches EOF.
2. The client executes `fs::remove_all(output_dir / topic)` to wipe partial files.
3. The client sleeps with exponential backoff and connects anew with `GET <topic>`.
4. The server re-streams the entire topic from file 1, byte 0.

#### Part 2 Case 2 Recovery (Checkpoint Resume)
1. **Checkpointing:** During streaming, after every $1\text{ MiB}$ (or completed file), the client sends `CHECKPOINT <sid> <file> <offset>`. The server updates its internal state and replies with `ACK`.
2. **Atomic Disk Flush:** The client flushes bytes to disk (`file.flush()`), writes state to `.<topic>.checkpoint.tmp`, and performs an atomic filesystem rename to `.<topic>.checkpoint`.
3. **Resumption:** On failure, the client reconnects and sends `RESUME <sid> <topic> <file> <offset>`.
4. **Validation:** The server verifies the session exists, validates that `offset <= validated_checkpoint_offset`, and replies with `RESUME_OK`.
5. **Random-Access Resume:** The client opens the existing partial file with `std::ios::in | std::ios::out | std::ios::binary` and positions the write head with `file.seekp(offset)`. Completed files recorded in `completed_files` are skipped entirely.
6. **Completion:** Upon receiving `END`, the client deletes the checkpoint file.

#### Part 2 Case 2 Enhanced Recovery (Partitioned Concurrency)
- Each parallel worker thread operates independently on its own socket.
- Partition $i$ maintains its own local checkpoint file: `output_dir/.<topic>.part<i>.checkpoint`.
- If connection 1 drops, connection 0 and connection 2 continue downloading unaffected. Worker 1 reconnects, sends `RESUME` for partition 1, and resumes only its remaining assigned files.
- Because files assigned to partitions are disjoint ($index \pmod N$), threads write to distinct files without mutex contention or reassembly collisions.

#### Server Restart Limitation & Mitigation
The server maintains session state in-memory (`std::map<std::string, ServerSession>`). If the server process crashes and restarts, session state is cleared:
- A reconnecting client sending `RESUME` receives `ERROR BAD_SESSION unknown_session_id`.
- The client gracefully catches `BAD_SESSION`, deletes its stale `.checkpoint` file, and falls back to restarting the topic transfer from scratch.

---

## 8. Performance Benchmarking and Results Directory

The `Results/` directory contains tools and logs for empirical performance comparison:
- [`Results/results_template.csv`](file:///c:/Users/patel/Downloads/FinalSubmission_Group_056_069/FinalSubmission_Group_056_069/FinalSubmission_Group_056_069/Results/results_template.csv): Standard CSV header for benchmark logging:
  ```csv
  case,topic,clients,failure_probability,attempt,total_transmitted_bytes,useful_bytes,redundant_bytes,completion_seconds,throughput_Bps,failures,reconnects
  ```
- [`Results/run_experiment.sh`](file:///c:/Users/patel/Downloads/FinalSubmission_Group_056_069/FinalSubmission_Group_056_069/FinalSubmission_Group_056_069/Results/run_experiment.sh): Bash automation script to launch concurrent client processes and measure wall-clock completion time.

### Running Concurrency Benchmarks
To run the benchmarking harness:
```sh
cd Results
chmod +x run_experiment.sh

# Usage: ./run_experiment.sh <case_dir> <server_ip> <port> <topic> <output_root> [client_counts]
./run_experiment.sh ../Part1 127.0.0.1 5000 cane ./experiment_out "1 2 4 8"
```

### Metrics Recorded
- **Useful Bytes:** Raw payload bytes written to valid topic images.
- **Total Transmitted Bytes:** All bytes sent across the wire (useful bytes + headers + redundant retransmissions).
- **Redundant Bytes:** $\text{Total Bytes} - \text{Useful Bytes}$ (measures retransmission overhead caused by failures).
- **Completion Time:** Elapsed seconds from initial request to final completion.
- **Throughput:** $\text{Useful Bytes} / \text{Completion Time}$ (in B/s or MB/s).

---

## 9. Troubleshooting Guide

| Problem | Root Cause | Solution |
| :--- | :--- | :--- |
| `bind: Address already in use` | Previous server instance still holds the port. | Find and terminate the process: `lsof -i :<port>` or `ss -lptn 'sport = :<port>'`, then `kill -9 <PID>`. Alternatively, wait 30s for `TIME_WAIT` or change the port. |
| `Connection refused` | Server is not running or listening on a different port/interface. | Verify the server is running. Check IP using `127.0.0.1` locally or `hostname -I` across WSL/LAN. |
| `ERROR UNKNOWN_TOPIC` | Requested topic folder does not exist under `topics_root_dir`. | Check directory name spelling. In Animals-10, directories are lowercase Italian names (`cane`, `gatto`, `mucca`, etc.). |
| `ERROR BAD_REQUEST expected_GET_topic` | Malformed initial message or mismatched client-server versions. | Ensure the client and server binaries are built from the same directory case. |
| `ERROR BAD_SESSION unknown_session_id` | Server was restarted, wiping its in-memory session map. | Client automatically deletes `.checkpoint` and restarts transfer. To manually reset, delete `output_dir/.*.checkpoint`. |
| `Maximum reconnect attempts reached` | Failure probability $p$ is too high for the connection to complete 1 chunk before dropping. | Lower `failure_probability` (e.g., test with `0.005` or `0.01` instead of `0.2`), or increase retries: `export IT305_MAX_RECONNECTS=200`. |
| `Permission denied` when building or running | Missing execute permissions on scripts or output directories. | Run `chmod +x run_experiment.sh` or check file ownership: `chown -R $USER .`. |

---

## 10. Design Document Compliance & Documented Deviations

1. **Part 1 Framing:** Section 3.2 specifies clean binary streaming without chunk headers (`FILE <len> <size>\n` followed by raw data). This is matched in `Part1/server.cpp` and `Part1/client.cpp`.
2. **Part 2 Case 1 Framing:** The Design Document (Section 5) specified that Case 1 reuse the exact Part 1 message format. The current implementation in `Part2_Case1/` uses explicit `START` and `DATA <pos> <got>` chunk headers. This was introduced to provide chunk-level integrity verification across failures.
3. **Part 2 Case 2 Enhanced Protocol:** Section 7.1 outlined a dynamic negotiation channel (`OPEN`, `PARTITION`, `JOIN`). The implemented codebase uses a direct, deterministic whole-file modulo partitioning approach (`index % parts == part`) with independent session tracking. This simplifies reassembly, eliminates control channel bottlenecks, and guarantees zero write collisions on disk.
4. **Server State Volatility:** Per Design Document Section 6.2, sessions are stored in an in-memory `std::map` protected by `std::mutex`. Server restarts drop active sessions; client-side fallback recovers gracefully via `BAD_SESSION` detection.
