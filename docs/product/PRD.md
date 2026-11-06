# Product Requirements & Vision — Dibyt

**Product Name:** Dibyt  
**CLI Name:** `dibyt`  
**Tagline:** Move data. Recover by design.  
**Document Status:** Approved Product Specification (v0.1 Planning)

---

## 1. Product Mission

Dibyt is a lightweight, high-performance, self-hosted file distribution system. A sender exposes a collection of files, and one or more receivers fetch that collection concurrently over TCP.

Dibyt is built on a single core principle: **Correctness before speed. A fast corrupted transfer is a failed transfer.**

Dibyt eliminates the fragility of traditional file transfers (such as raw scp or naive socket copies) across unstable networks by introducing **durable session checkpoints**, **parallel TCP worker streams**, **dynamic work scheduling**, and **mandatory cryptographic integrity verification**.

---

## 2. Problem Statement

Organizations, developers, and researchers frequently need to distribute multi-gigabyte collections of assets, model checkpoints, build artifacts, or dataset archives across private networks, edge infrastructure, or distributed development rigs.

Common operational failure modes today:
1. **Network Interruptions & Flaky Links:** A single dropped connection during an 80 GB transfer forces naive tools to re-transfer all bytes from scratch.
2. **Crash & Reboot Vulnerability:** If a sending daemon or receiving process halts mid-transfer, state stored in RAM is lost.
3. **Imbalanced Parallelism:** Naive modulo-based partitioning (`file_index % workers`) assigns massive files to one worker while others idle.
4. **Silent Corruption & Unsafe Finalization:** Partial files written directly into output paths are read by downstream applications before integrity is verified.
5. **Over-Engineering:** Users are forced to deploy distributed object stores (MinIO, Ceph) or cloud buckets (S3) just to copy files between internal machines reliably.

Dibyt solves these problems with a focused, self-hosted single-binary modular architecture.

---

## 3. Product Goals & Non-Goals

### Goals (v0.1 Developer Preview)
- **Generic File & Directory Collections:** Distribute arbitrary directory structures preserving relative paths, permissions, and file metadata.
- **Resilient Resume by Design:** Checkpoints are backed by receiver-side durable storage (SQLite) and flushed to disk before acknowledgement.
- **Parallel TCP Worker Pool:** Bounded worker connections per transfer with dynamic task leasing to saturate high-bandwidth, high-latency links.
- **Fault-Tolerant Worker Recovery:** Failure of one worker connection does not abort the transfer; remaining workers reclaim the task lease.
- **Strict End-to-End Integrity:** OpenSSL EVP SHA-256 verification of every file before atomic rename into destination roots.
- **First-Class CLI Workflow:** Human-friendly terminal UI with progress bars, structured JSON logging option, and standardized exit codes.
- **Target Scale:** Reliable operation across 10–100 concurrent peer connections.

### Non-Goals (Out of Scope for v0.1)
- **Public Cloud Storage Replacement:** Dibyt is not an AWS S3, Google Cloud Storage, or Azure Blob clone.
- **Peer-to-Peer (BitTorrent-style) Swarming:** Receivers download directly from senders; peer-to-peer relay swarming is explicitly out of scope.
- **Two-Way Synchronization / Continuous Sync:** Dibyt is not Dropbox or Syncthing. It performs directed distribution of immutable snapshot manifests.
- **Distributed Database / Clustered Raft:** Dibyt does not require distributed consensus or multi-node database clusters.
- **Payload Proxying via HTTP:** Management APIs provide control-plane coordination only; raw file payloads never pass through HTTP/REST layers.

---

## 4. User Personas & Core Use Cases

### Personas
1. **DevOps & Infrastructure Engineer (Alex):**
   - *Needs:* Automated distribution of Docker image tarballs, compiler caches, and binary artifacts across bare-metal build clusters.
   - *Pain point:* SCP commands fail randomly over WAN links and lack automatic resumption.
2. **Machine Learning Researcher (Priya):**
   - *Needs:* Syncing 50 GB dataset partitions and model weights from a central workstation to remote GPU compute instances.
   - *Pain point:* Python transfer scripts fail halfway through, requiring manual cleanup and re-downloads.
3. **Embedded Systems Integrator (Carlos):**
   - *Needs:* Reliable firmware image push to edge gateways over intermittent cellular/Wi-Fi links.
   - *Pain point:* Low memory on devices cannot tolerate unbounded thread pools or in-memory chunk caches.

### Core User Flows
- **Publish a Collection:**
  ```bash
  dibyt publish --path /data/dataset-v1 --listen 0.0.0.0:9080 --name dataset-v1
  ```
- **Inspect Manifest:**
  ```bash
  dibyt manifest get --host sender.local:9080 --collection dataset-v1
  ```
- **Pull with Parallel Streams & Resume:**
  ```bash
  dibyt fetch --host sender.local:9080 --collection dataset-v1 --dest /local/data --workers 4
  ```
- **Resume After Crash:**
  ```bash
  dibyt fetch --resume --session-id <uuid>
  ```

---

## 5. Canonical Product Terminology

- **Collection:** A logical named bundle of files and relative directory trees scanned from a designated host root.
- **Manifest:** An immutable, deterministically sorted catalog of files in a collection, specifying relative paths, sizes, and cryptographic SHA-256 hashes.
- **Transfer Session:** A stateful, durable transaction between a sender and receiver tracking the transfer of a specific manifest.
- **Task:** An atomic unit of transfer work representing a whole file (or in later sprints, a large file chunk).
- **Task Lease:** A time-bounded assignment of a task to a specific worker connection, guarded by heartbeats and expiration.
- **Checkpoint:** A durable marker indicating the exact byte offset in a file that has been written, synced to storage, and confirmed by the receiver.
- **Quarantine / Staging Area:** A private directory where incoming files are written (`.dibyt-tmp-<uuid>`) until fully received and verified.
- **Finalization:** The atomic movement of a verified file from the staging area into its final target path within the destination directory.
