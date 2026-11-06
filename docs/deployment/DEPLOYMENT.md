# Deployment Guide (Proposed) — Dibyt

**Document Status:** Proposed Specification (Target Milestones: Sprints 14, 19)

---

## 1. Overview

Dibyt is designed for self-hosted operation on bare-metal servers, virtual machines, and containerized edge nodes. As a modular monolith, Dibyt does not require external microservices, background message queues, or standalone database clusters.

---

## 2. Deployment Models

### Model A: Standalone Host Binary (Direct Systemd Service)
Ideal for maximum raw network performance without container overhead.
- Binaries: `dibyt` (unified CLI and daemon)
- Persistent Storage: Local NVMe/SSD directory for SQLite database and temporary file staging.
- Network: Dedicated high-throughput TCP port (default: `9080` for data plane, `9081` for management API).

Example `systemd` service unit:
```ini
[Unit]
Description=Dibyt File Distribution Daemon
After=network.target

[Service]
Type=simple
User=dibyt
Group=dibyt
ExecStart=/usr/local/bin/dibyt daemon --config /etc/dibyt/dibyt.conf
Restart=always
RestartSec=5s
LimitNOFILE=65536
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
```

### Model B: Docker Container & Docker Compose
Ideal for automated deployment in test environments and CI/CD pipelines.
- Base Image: Minimal Alpine Linux or Debian slim with glibc.
- Volumes:
  - `/var/lib/dibyt/data`: SQLite database and state persistence.
  - `/var/lib/dibyt/staging`: Temporary quarantine and staging directory (must reside on the same filesystem as target destination).
  - `/data`: Read-only source collections or destination target folders.

---

## 3. Network Exposure & Security Policies

1. **Private Network Default:** Bind to `127.0.0.1` or internal private VPN/VPC subnets (`10.0.0.0/8`, `192.168.0.0/16`).
2. **Public Exposure Requirements:**
   - Never expose unauthenticated plain TCP sockets directly to the public internet.
   - Require TLS encryption and client authentication (Sprint 15) before enabling public ingress.
   - Configure firewall rules (`iptables` / `nftables` / Cloud Security Groups) to restrict ingress traffic strictly to authorized receiver IP ranges.
