# Deployment & Self-Hosting Guide — Dibyt

This guide covers deployment architectures, container workflows, systemd configuration, network boundaries, security hardening, and disaster recovery for Dibyt.

---

## 1. Architectural Topology & Plane Separation

Dibyt maintains strict separation between administrative management and bulk data movement:

| Plane | Protocol | Default Port | Exposure | Description |
| :--- | :--- | :--- | :--- | :--- |
| **Control Plane** | HTTP / JSON | `8080` | Private / Internal (`127.0.0.1` or VPN) | `/health`, `/metrics`, `/api/v1` collections and transfer state management. |
| **Data Plane** | Raw TCP (Binary Framing) | `9080` | Public / Ingress | High-throughput binary framing protocol for parallel chunk streaming. |

> [!IMPORTANT]
> The raw TCP transfer port (`9080`) requires direct layer-4 network ingress. Layer-7 HTTP proxies (e.g., standard Cloudflare CDN proxies or standard web PaaS routing) **cannot** terminate Dibyt's custom binary protocol. Use a Linux VPS, bare-metal server, or cloud provider with layer-4 load balancing / public IP access.

---

## 2. Docker Compose Deployment

The recommended self-hosted method utilizes Docker Compose with dedicated volumes and drop-capabilities security options:

```bash
# Clone the repository and navigate to deploy/compose
cd deploy/compose

# Review and edit production environment variables
cp config/dibyt.env config/dibyt.env.local

# Launch Dibyt Server and Web Console in detached mode
docker compose up -d
```

### Persistent Volumes
- `dibyt_data`: Persists SQLite state (`dibyt.db`), transaction WAL journals, durable checkpoint streams, and finalized collection files.
- Configuration is mounted read-only (`:ro`) at `/etc/dibyt/dibyt.env`.

---

## 3. Native Linux systemd Service

For bare-metal and Linux VPS installations (e.g., Ubuntu 22.04 / 24.04, Debian 12):

### 1. User & Directory Setup
```bash
sudo useradd --system --shell /usr/sbin/nologin --home-dir /var/lib/dibyt dibyt
sudo mkdir -p /var/lib/dibyt/data /var/lib/dibyt/staging /var/log/dibyt /etc/dibyt
sudo chown -R dibyt:dibyt /var/lib/dibyt /var/log/dibyt
```

### 2. Binary Installation
```bash
sudo install -m 0755 build/apps/dibyt-server/dibyt-server /usr/local/bin/dibyt-server
sudo install -m 0755 build/apps/dibyt-cli/dibyt-cli /usr/local/bin/dibyt-cli
```

### 3. Service Activation
```bash
sudo cp deploy/systemd/dibyt.service /etc/systemd/system/dibyt.service
sudo systemctl daemon-reload
sudo systemctl enable --now dibyt
sudo systemctl status dibyt
```

---

## 4. Firewall & Network Ingress Rules

Using `ufw` on Linux:

```bash
# Allow SSH access
sudo ufw allow 22/tcp

# Data Plane: Allow public raw TCP ingress for parallel data transfers
sudo ufw allow 9080/tcp comment "Dibyt Data Plane (TCP Transfer)"

# Control Plane: Restrict API access to local loopback or private subnet
sudo ufw allow from 10.0.0.0/8 to any port 8080 proto tcp comment "Dibyt Control Plane (Internal)"
# Alternatively, reverse-proxy HTTP through Nginx or Caddy with TLS:
# sudo ufw allow 443/tcp comment "HTTPS Reverse Proxy"

sudo ufw enable
```

---

## 5. Backup & Disaster Recovery

### SQLite & State Backup
Dibyt's embedded SQLite database uses WAL mode. To safely back up without stopping the server:

```bash
# Safely dump or copy the SQLite database
sqlite3 /var/lib/dibyt/dibyt.db ".backup '/var/backups/dibyt-$(date +%F).db'"

# Archive checkpoint files and configuration
tar -czf /var/backups/dibyt-config-$(date +%F).tar.gz /etc/dibyt/
```

### Recovery Procedure
1. Stop the daemon: `sudo systemctl stop dibyt` (or `docker compose down`).
2. Restore the database file: `cp /var/backups/dibyt-YYYY-MM-DD.db /var/lib/dibyt/dibyt.db`.
3. Check database consistency: `sqlite3 /var/lib/dibyt/dibyt.db "PRAGMA integrity_check;"`.
4. Restart the service: `sudo systemctl start dibyt`.
