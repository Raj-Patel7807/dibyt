# Production Readiness Checklist — Dibyt

**Classification:** Developer Preview / Pre-Release Gate  
**Target Release:** v0.1.0-dev  
**Maintainer:** Raj Patel  

---

## 1. Quality & Code Assurance Gate

- [x] **C++20 Compliance:** Pure ISO C++20 standard library with zero external runtime dependencies.
- [x] **Unit & Regression Testing:** 14 unit test suites passing cleanly across all library modules (`libs/core`, `libs/manifest`, `libs/protocol`, `libs/networking`, `libs/storage`, `libs/transfer`, `libs/scheduler`, `libs/observability`, `libs/security`, `apps/dibyt-api`, `apps/dibyt-cli`).
- [x] **Fault-Injection & Chaos Testing:** Bounded retry, chunk corruption quarantine, and simulated network disconnect recovery verified (`test_fault_injection`).
- [x] **Benchmark Validation:** Reproducible throughput benchmark harness runs end-to-end multi-worker transfers with SHA-256 validation (`benchmark_runner`).
- [x] **Compiler Diagnostics:** Built with high warning levels (`-Wall -Wextra -Wpedantic`), zero warnings as errors.
- [x] **Clean Tree Hygiene:** No leftover object files, debug binaries, or temporary test artifacts in the repository.

---

## 2. Security & Boundaries Gate

- [x] **Zero Plaintext Secrets:** No credentials, private tokens, or sensitive API keys in code or Git history.
- [x] **Constant-Time Token Comparison:** `dibyt::security::constant_time_equals` used for credential matching to prevent timing attacks.
- [x] **Filesystem Jail Isolation:** Directory traversal vectors (`..`, symbolic links, root escapes) blocked by `FilesystemJail` and `sanitize_relative_path`.
- [x] **Unprivileged Containers:** Docker image drops all Linux capabilities (`cap_drop: ALL`), enforces `no-new-privileges: true`, and executes as non-root user `dibyt` (UID 10001).
- [x] **Network Port Separation:** Management plane (HTTP `8080`) is isolated from high-speed bulk data plane (TCP `9080`).

---

## 3. Operational & Deployment Gate

- [x] **Persistent Storage:** SQLite databases (`dibyt.db`), checkpoint streams, and data stores reside in dedicated volume mounts (`dibyt_data`).
- [x] **Read-Only Configuration:** Daemon configuration mounted read-only (`:ro`) at runtime.
- [x] **Health Check Endpoint:** Real HTTP health probe (`GET /health`) implemented in API router and Docker container definitions.
- [x] **Graceful Shutdown:** SIGTERM handler coordinates in-flight chunk flush, lease relinquishment, and socket closure before daemon exit.
- [x] **systemd Sandboxing:** Native Linux systemd service unit includes `ProtectSystem=strict`, `ProtectHome=true`, `PrivateTmp=true`, and `MemoryDenyWriteExecute=true`.
- [x] **Automated CI/CD:** GitHub Actions workflow verifies multi-stage builds, tests on every push/PR, and gates release tarballs and SHA-256 checksums on approved version tags (`v*.*.*`).

---

## 4. Release Decision

- [ ] **Enterprise Multi-Tenant Production Ready** (Blocked on production mTLS integration and multi-node clustering)
- [x] **Developer Preview / Self-Hosted Single-Node Ready** (Approved for standalone Linux VPS or developer environments)
