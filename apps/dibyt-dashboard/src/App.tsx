import React, { useState } from 'react';
import type { EngineStatusResponse } from './types.ts';

export const App: React.FC = () => {
  const [status, setStatus] = useState<EngineStatusResponse | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [token, setToken] = useState<string>('');
  const [loading, setLoading] = useState<boolean>(false);

  const fetchStatus = async () => {
    setLoading(true);
    setError(null);
    try {
      const res = await fetch('http://127.0.0.1:9081/api/v1/status', {
        headers: {
          Authorization: `Bearer ${token}`,
        },
      });

      if (!res.ok) {
        throw new Error(`HTTP Error: ${res.status} ${res.statusText}`);
      }

      const data: EngineStatusResponse = await res.json();
      setStatus(data);
    } catch (err: unknown) {
      setError(err instanceof Error ? err.message : 'Failed to query management API');
    } finally {
      setLoading(false);
    }
  };

  return (
    <div style={{ padding: '2rem', maxWidth: '800px', margin: '0 auto' }}>
      <header style={{ borderBottom: '1px solid #334155', paddingBottom: '1rem', marginBottom: '2rem' }}>
        <h1 style={{ margin: 0, fontSize: '1.75rem', color: '#38bdf8' }}>Dibyt Management Console</h1>
        <p style={{ margin: '0.25rem 0 0', color: '#94a3b8' }}>Real-time Transfer Monitoring & Telemetry</p>
      </header>

      <section style={{ background: '#1e293b', padding: '1.5rem', borderRadius: '8px', marginBottom: '1.5rem' }}>
        <h2 style={{ fontSize: '1.1rem', marginTop: 0 }}>API Authentication</h2>
        <div style={{ display: 'flex', gap: '0.5rem' }}>
          <input
            type="password"
            placeholder="Enter Bearer Token"
            value={token}
            onChange={(e) => setToken(e.target.value)}
            style={{
              flex: 1,
              padding: '0.5rem 0.75rem',
              borderRadius: '4px',
              border: '1px solid #475569',
              background: '#0f172a',
              color: '#f8fafc',
            }}
          />
          <button
            onClick={fetchStatus}
            disabled={loading || !token}
            style={{
              padding: '0.5rem 1rem',
              borderRadius: '4px',
              border: 'none',
              background: '#0284c7',
              color: '#fff',
              cursor: 'pointer',
              fontWeight: 500,
            }}
          >
            {loading ? 'Refreshing...' : 'Query Engine'}
          </button>
        </div>
      </section>

      {error && (
        <div style={{ background: '#7f1d1d', color: '#fecaca', padding: '1rem', borderRadius: '6px', marginBottom: '1.5rem' }}>
          <strong>Error:</strong> {error}
        </div>
      )}

      {status && (
        <section style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fit, minmax(200px, 1fr))', gap: '1rem' }}>
          <div style={{ background: '#1e293b', padding: '1rem', borderRadius: '8px' }}>
            <span style={{ color: '#94a3b8', fontSize: '0.85rem' }}>Total Bytes Transferred</span>
            <div style={{ fontSize: '1.5rem', fontWeight: 600, marginTop: '0.25rem' }}>{status.total_bytes_transferred}</div>
          </div>
          <div style={{ background: '#1e293b', padding: '1rem', borderRadius: '8px' }}>
            <span style={{ color: '#94a3b8', fontSize: '0.85rem' }}>Verified Files Completed</span>
            <div style={{ fontSize: '1.5rem', fontWeight: 600, marginTop: '0.25rem', color: '#4ade80' }}>{status.total_files_completed}</div>
          </div>
          <div style={{ background: '#1e293b', padding: '1rem', borderRadius: '8px' }}>
            <span style={{ color: '#94a3b8', fontSize: '0.85rem' }}>Total Retries</span>
            <div style={{ fontSize: '1.5rem', fontWeight: 600, marginTop: '0.25rem', color: '#facc15' }}>{status.total_retries}</div>
          </div>
        </section>
      )}
    </div>
  );
};
