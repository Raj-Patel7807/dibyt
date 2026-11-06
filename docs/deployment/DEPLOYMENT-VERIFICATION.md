# Deployment Verification & Smoke Test Drill — Dibyt

This document details the live verification drill for deploying Dibyt on a clean Linux environment (bare-metal VPS or Docker host).

---

## 1. Prerequisites & Environment Verification

| Requirement | Specification | Verification Command |
| :--- | :--- | :--- |
| **Operating System** | Linux (Ubuntu 22.04 LTS+, Debian 12+) | `uname -srm` |
| **Container Engine** | Docker Engine 24.0+ & Docker Compose v2.20+ | `docker compose version` |
| **Firewall (UFW)** | Layer-4 TCP port 9080 open; port 8080 restricted | `sudo ufw status verbose` |
| **Storage Mount** | Persistent block storage mounted at `/var/lib/dibyt` | `df -h /var/lib/dibyt` |

---

## 2. Docker Compose Verification Drill

### Step 1: Clone and Inspect Configuration
```bash
git clone https://github.com/rajpatel/dibyt.git
cd dibyt/deploy/compose
cat config/dibyt.env
```

### Step 2: Start Containers in Detached Mode
```bash
docker compose up -d
docker compose ps
```
*Expected Outcome:* Both `dibyt-server` and `dibyt-dashboard` containers report `Up (healthy)`.

### Step 3: Health Endpoint Smoke Test
```bash
curl -i http://127.0.0.1:8080/health
```
*Expected Response:*
```http
HTTP/1.1 200 OK
Content-Type: application/json

{"product":"Dibyt","status":"HEALTHY","version":"0.1.0"}
```

### Step 4: Data Persistence Drill
1. Trigger a transfer session creating files in `/var/lib/dibyt/data/`.
2. Inspect the local SQLite database inside the container:
   ```bash
   docker compose exec dibyt-server ls -la /var/lib/dibyt/data/
   ```
3. Restart the container stack:
   ```bash
   docker compose restart
   ```
4. Verify database records and active checkpoints remain intact.

---

## 3. Disaster Recovery & Backup Smoke Drill

### Step 1: Database Hot Backup
```bash
docker compose exec dibyt-server sqlite3 /var/lib/dibyt/dibyt.db ".backup '/var/lib/dibyt/backup_test.db'"
```

### Step 2: Integrity Verification
```bash
docker compose exec dibyt-server sqlite3 /var/lib/dibyt/backup_test.db "PRAGMA integrity_check;"
```
*Expected Output:* `ok`.

### Step 3: Clean State Reversion
```bash
docker compose down -v  # Removes test volumes if performing clean teardown
```
