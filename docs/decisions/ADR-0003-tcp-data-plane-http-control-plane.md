# ADR-0003: Control-Plane (HTTP) and Data-Plane (TCP) Separation

**Status:** Proposed  
**Date:** 2026-11-06  
**Deciders:** Raj Patel (Project Maintainer)  

---

## Context and Problem Statement
Dibyt provides both high-speed bulk data transfer and operational management (session creation, metrics, health status, and web dashboard integration). Routing multi-gigabyte binary file streams through HTTP/JSON REST APIs incurs severe serialization and buffering overhead. Conversely, using raw TCP sockets for RESTful control commands complicates dashboard and orchestration integrations.

---

## Decision Drivers
- High throughput and zero-copy streaming for large file payloads.
- Easy integration with standard web dashboards, monitoring tools, and orchestrators.
- Clear security boundaries between data plane sockets and administrative management endpoints.

---

## Considered Options
1. **Unified HTTP/REST for All Traffic:** Transferring files as HTTP multi-part or streaming octet-stream payloads over REST.
2. **Unified Raw TCP for All Traffic:** Implementing custom framing for both binary data and administrative commands.
3. **Dedicated Control Plane (HTTP) & Data Plane (TCP) (Chosen):** Using raw TCP with custom framing for data transfer, and standard HTTP/JSON for management and telemetry.

---

## Decision Outcome
Chosen option: **Dedicated Control Plane (HTTP) & Data Plane (TCP)**.

### Positive Consequences
- **Maximum Data Throughput:** Data plane TCP streams operate with raw binary framing, 64 KiB chunks, and minimal protocol overhead.
- **Ecosystem Compatibility:** Management endpoints (`/health`, `/metrics`, `/api/v1/sessions`) can be scraped directly by Prometheus and consumed by React web dashboards.
- **Strict Boundary:** Bulk file data never passes through HTTP layers.

### Negative Consequences / Trade-offs
- Requires listening on two separate network ports (e.g., port 9080 for data plane TCP, port 9081 for management HTTP).
