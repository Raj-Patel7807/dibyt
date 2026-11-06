# Operational Runbook (Proposed) — Dibyt

**Document Status:** Proposed Operational Runbook (Target Milestones: Sprints 14, 19)

---

## 1. Routine Operations

### Checking Service Health
- **CLI Diagnostic:**
  ```bash
  dibyt status --all
  ```
- **HTTP Management Health Endpoint:**
  ```bash
  curl -s http://127.0.0.1:9081/health | jq .
  ```
  Expected output:
  ```json
  {
    "status": "healthy",
    "version": "0.1.0-dev",
    "uptime_seconds": 84210,
    "active_transfers": 2,
    "active_workers": 6,
    "sqlite_status": "ok"
  }
  ```

---

## 2. SQLite Database Maintenance & Backups

Dibyt persists all session and transfer state in a local SQLite database (`dibyt.db`).

### Safe Online Backup
Because SQLite uses Write-Ahead Logging (WAL) mode, online backups can be executed safely without stopping the daemon using the SQLite Online Backup API or CLI:
```bash
sqlite3 /var/lib/dibyt/data/dibyt.db ".backup '/var/backups/dibyt/dibyt-$(date +%F).db'"
```

### Database Recovery After Crash
In the event of an abrupt host reboot or power outage:
1. SQLite WAL replay automatically recovers committed transactions upon daemon restart.
2. The Dibyt scheduler inspects in-flight tasks and checks:
   - Tasks in `LEASED` status whose worker connections are dead are reset to `UNASSIGNED`.
   - Durable byte offsets stored in SQLite are treated as the resumption baseline.

---

## 3. Troubleshooting Common Failure Modes

### Issue 1: Transfer Stalls Mid-Stream
- **Symptom:** Receiver CLI stops showing progress; worker states remain idle.
- **Diagnostic:**
  - Check `dibyt status --session <session-id>` for active lease expirations.
  - Inspect server network logs for broken pipe (`EPIPE`) or connection reset (`ECONNRESET`).
- **Remediation:**
  - Heartbeat timeout will expire stale worker leases after 15 seconds.
  - If a worker process died, launch replacement workers using `dibyt fetch --resume --session-id <session-id>`.

### Issue 2: SHA-256 Integrity Verification Fails
- **Symptom:** Receiver log reports `ERR_INTEGRITY_MISMATCH` for a specific file.
- **Diagnostic:**
  - Inspect network hardware for silent packet corruption.
  - Review SQLite task error count.
- **Remediation:**
  - The staging file is automatically purged.
  - The task is reset to `PENDING` and re-fetched from byte 0 up to configured retry limits.
