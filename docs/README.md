# Dibyt Documentation Directory

Welcome to the internal engineering documentation for **Dibyt**, a resilient, parallel, and resumable self-hosted file distribution system.

This documentation tree is structured to provide clear, actionable specifications for developers, operators, and evaluators.

---

## Documentation Sections

### 1. Product & Vision
- [Product Requirements & Vision](product/PRD.md) — Mission, goals, non-goals, user personas, release scope, and canonical terminology.

### 2. Architecture & Design
- [Architecture Overview](architecture/ARCHITECTURE.md) — System boundaries, modular monolith organization, control plane vs. data plane separation, and concurrency targets.
- [Historical Prototype Analysis](design/LEGACY_ANALYSIS.md) — Deep dive into the four original academic stages (`Part1`, `Part2_Case1`, `Part2_Case2`, `Part2_Case2_Enhanced`), their demonstrated behavior, limitations, and key differences from Dibyt.
- [Checkpoint & Resume Semantics](design/CHECKPOINT_RESUME.md) — Durable byte accounting, receiver authority, flush-before-ACK invariants, and crash consistency.
- [Dynamic Scheduler Design](design/SCHEDULER.md) — Leased task queue model, work-stealing, fairness, and heartbeat recovery.
- [Integrity & Finalization](design/INTEGRITY.md) — SHA-256 validation pipelines, temporary file staging, and atomic directory commit.

### 3. Protocol Specification
- [Binary Protocol Specification (Draft)](protocol/SPECIFICATION.md) — Wire framing, fixed headers, magic bytes, length boundaries, state machine, and error signaling.

### 4. Development & Engineering Standards
- [Development Standards & Guidelines](development/STANDARDS.md) — C++20 conventions, RAII invariants, memory boundaries, error handling, and test-first policy.
- [Testing Strategy](development/TESTING.md) — Unit, integration, fault injection, and end-to-end testing topology.

### 5. Security & Threat Modeling
- [Security Threat Model](security/THREAT_MODEL.md) — Threat landscape, filesystem sandboxing, canonical path isolation, DoS mitigation, and security release gates.

### 6. Operations & Deployment
- [Deployment Guide (Proposed)](deployment/DEPLOYMENT.md) — Standalone binaries, Docker containers, systemd service units, and network exposure policies.
- [Operational Runbook (Proposed)](operations/RUNBOOK.md) — Health diagnostics, troubleshooting procedures, backup/restore for SQLite databases, and metric scraping.

### 7. Planning & Decisions
- [Roadmap (Sprints 01–20)](planning/ROADMAP.md) — Detailed engineering sprint breakdown and status tracking.
- [Architecture Decision Records (ADRs)](decisions/README.md) — Consolidated architectural decisions:
  - [01-ARCHITECTURE-DECISIONS.md](decisions/01-ARCHITECTURE-DECISIONS.md) (ADR-0001 through ADR-0005)
  - [ADR Template](decisions/ADR-TEMPLATE.md)

### 8. Legal & Rights
- [Legal, Licensing, and Rights Inventory](LEGAL_AND_RIGHTS.md) — Rights breakdown between new Dibyt code and historical coursework assets.

