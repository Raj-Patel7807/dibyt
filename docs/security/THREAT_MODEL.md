# Security Threat Model & Release Gates — Dibyt

**Document Status:** Approved Security Specification (v0.1)

---

## 1. Threat Landscape & Assumptions

Dibyt is designed to distribute data across local networks, cross-data-center interconnects, and potentially public WANs. When exposed to untrusted or semi-trusted networks, Dibyt treats all incoming network streams as potentially hostile.

### Untrusted Inputs:
1. TCP wire frames (malicious headers, manipulated lengths, corrupted payloads).
2. Remote manifest paths (directory traversal payloads, symlink attacks).
3. Client-supplied session IDs, worker IDs, and offset claims.
4. HTTP control-plane API payloads.

---

## 2. Identified Threats & Mitigations

| Threat ID | Threat Description | Attack Vector | Dibyt Mitigation Strategy |
|---|---|---|---|
| **THREAT-01** | **Directory Traversal** | Attacker crafts a manifest with paths like `../../etc/shadow` or `C:\Windows\System32\cmd.exe`. | **Canonical Jail Sandboxing:** Receiver normalizes all paths relative to the destination root (`std::filesystem::weakly_canonical`). Paths containing parent traversal `..` or escaping the root boundary are rejected immediately. |
| **THREAT-02** | **Symlink Escape Attack** | Manifest or source directory contains symlinks pointing outside the published collection tree. | **Strict Symlink Policy:** By default, symlinks are not resolved or followed. If symlinks are enabled via configuration, their resolved target must reside within the jail boundary. |
| **THREAT-03** | **Memory Exhaustion (DoS)** | Attacker sends a frame header declaring a 4 GB payload length to trigger massive memory allocation. | **Strict Frame Limits:** Payload length field is validated before allocation. Any frame declaring $> 16\text{ MiB}$ causes immediate socket termination without memory allocation. |
| **THREAT-04** | **Unbounded Worker Starvation** | Rogue client connects hundreds of parallel worker sockets to exhaust server file descriptors. | **Bounded Connection Pools:** The server enforces a strict global and per-session worker connection limit with connection timeouts and IP rate-limiting. |
| **THREAT-05** | **Man-in-the-Middle (MitM) Tampering** | Active network attacker modifies chunk bytes in flight to alter file contents. | **Cryptographic SHA-256 Verification & TLS:** Chunks are written to staging files. The final file is accepted only if the full SHA-256 matches the authentic manifest. In Sprint 15, mutual TLS encryption is enabled. |
| **THREAT-06** | **Unconfirmed Checkpoint Fraud** | Malicious client reports false durable offsets to trick the sender into skipping crucial bytes. | **Receiver Authority & Integrity Check:** The receiver only requests the bytes it has stored. If an offset skips bytes, the post-transfer SHA-256 verification will fail, quarantining the file. |
| **THREAT-07** | **Insecure Default Binding** | Daemon binds to `0.0.0.0` automatically, exposing unauthenticated endpoints to the public internet. | **Loopback-First Binding:** Default bind address is strictly `127.0.0.1` unless explicitly overridden via CLI flags or configuration. |

---

## 3. Mandatory Security Release Gates

Before any production or developer preview release (v0.1) is cleared:
1. **Fuzz Testing:** Protocol frame parser must undergo fuzz testing (via LLVM `libFuzzer` or AFL++) with zero unhandled memory violations.
2. **Filesystem Boundary Tests:** Integration test suite must include negative test cases for path traversal, symlink escapes, and null byte injections in file paths.
3. **No Real Secrets in Repository:** Automated scan ensuring zero hard-coded API tokens, private keys, or credentials in source, docs, or commit history.
4. **Leak Sanitizer & Valgrind Cleanliness:** Clean execution under Clang AddressSanitizer (`ASan`), UndefinedBehaviorSanitizer (`UBSan`), and ThreadSanitizer (`TSan`).
