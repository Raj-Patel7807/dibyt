# ADR-0005: Immutable Manifest Snapshot Policy

**Status:** Proposed  
**Date:** 2026-11-06  
**Deciders:** Raj Patel (Project Maintainer)  

---

## Context and Problem Statement
When a sender publishes a directory collection, the underlying files on the sender's filesystem could theoretically be modified, appended to, or deleted while an active transfer is underway. If a manifest is dynamic, receivers can experience race conditions, corrupted chunk ranges, and failing checksum verifications.

---

## Decision Drivers
- Deterministic transfer state across all parallel worker streams.
- Accurate SHA-256 integrity verification.
- Protection against source file mutations during transmission.

---

## Considered Options
1. **Live Directory Streaming:** Reading file contents dynamically without pre-computing hashes or manifests.
2. **Immutable Snapshot Manifest (Chosen):** Scanning and generating an immutable manifest (with exact file sizes and SHA-256 hashes) at collection creation time. The manifest is versioned and tied to the session.

---

## Decision Outcome
Chosen option: **Immutable Snapshot Manifest**.

### Positive Consequences
- **Deterministic Contract:** Senders and receivers agree on the exact set of files, byte counts, and expected hashes prior to starting data streaming.
- **Mutation Detection:** If a source file's modification time or size changes during the transfer, the sender aborts the task with `ERR_SOURCE_MUTATED`, preventing the transmission of corrupted or inconsistent data.
- **Reproducibility:** A transfer can be verified and audited cleanly.

### Negative Consequences / Trade-offs
- There is a pre-transfer scan latency to compute initial SHA-256 hashes for large collections.
