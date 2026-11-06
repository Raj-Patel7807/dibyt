# Dynamic Work Scheduler Design — Dibyt

**Document Status:** Approved Design Specification (v0.1)

---

## 1. Moving Beyond Static Modulo Partitioning

The legacy academic prototype assigned files to parallel TCP streams using static modulo division:
$$\text{worker\_id} = \text{file\_index} \pmod N$$

### Flaws of Static Modulo:
1. **Severe Head-of-Line Blocking:** If one worker is assigned a single 10 GB video file and another worker is assigned a hundred 10 KB files, worker 2 finishes in seconds while worker 1 runs alone for hours. Total transfer time is bottlenecked by the slowest single partition.
2. **Failure Coupling:** If worker 1 crashes, all remaining files scheduled for worker 1 stall unless a complex re-indexing scheme is invoked.

---

## 2. Dynamic Leased Task Queue Architecture

Dibyt replaces static assignment with a **centralized, dynamic, leased task queue** managed within the session engine:

```text
+-------------------------------------------------------------+
|                     Session Task Queue                      |
|                                                             |
|   [File 1 (10MB): UNASSIGNED]                               |
|   [File 2 (500MB): LEASED -> Worker 2 (Expires in 12s)]     |
|   [File 3 (50KB): LEASED -> Worker 1 (Expires in 8s)]       |
|   [File 4 (2GB): UNASSIGNED]                                |
|   [File 5 (12MB): COMPLETED]                                |
+-------------------------------------------------------------+
               ^                             ^
        Lease  |                      Lease  |
        Task   |                      Task   |
               v                             v
        +--------------+              +--------------+
        |   Worker 1   |              |   Worker 2   |
        | (Idle/Fast)  |              | (Busy/Slow)  |
        +--------------+              +--------------+
```

### Key Scheduler Rules:
1. **Worker Pull Model:** Workers do not have work pushed to them unconditionally. When a worker finishes a file (or starts up), it issues a `FRAME_LEASE_TASK_REQ` to the session scheduler.
2. **Task Ordering (LPT / Priority):** Unassigned tasks are sorted using Longest Processing Time (LPT) first or descending file size. Large files begin transferring early, while smaller files fill the remaining concurrency tail, minimizing overall job makespan.
3. **Lease Timeouts & Heartbeats:**
   - A lease is granted with a strict TTL (default: 15 seconds).
   - As long as a worker is streaming bytes or sending checkpoint ACKs, its lease is automatically refreshed.
   - If a worker disconnects, crashes, or fails to report progress within the TTL, the lease **expires**.
   - The scheduler resets expired tasks to `UNASSIGNED` (or `RETRY_PENDING`) so any idle worker can immediately claim it.
4. **Bounded Retries:** Each task maintains an attempt counter. If a task fails verification or encounters fatal I/O errors more than $N_{\text{max}}$ times (default: 5), the session halts or flags the file as failed, preventing infinite recovery loops.
