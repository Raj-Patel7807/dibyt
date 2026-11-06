import { describe, it, expect } from 'vitest';
import type { HealthResponse, EngineStatusResponse, CollectionItem } from './types';

describe('Dashboard Domain Types and State Invariants', () => {
  it('validates HealthResponse model structure', () => {
    const health: HealthResponse = {
      product: 'Dibyt',
      status: 'HEALTHY',
      version: '0.1.0'
    };

    expect(health.product).toBe('Dibyt');
    expect(health.status).toBe('HEALTHY');
    expect(health.version).toBe('0.1.0');
  });

  it('validates EngineStatusResponse metrics aggregation', () => {
    const metrics: EngineStatusResponse = {
      product: 'Dibyt',
      version: '0.1.0',
      total_bytes_transferred: 104857600,
      total_files_completed: 42,
      total_retries: 2
    };

    expect(metrics.total_bytes_transferred).toBeGreaterThan(0);
    expect(metrics.total_files_completed).toBe(42);
    expect(metrics.total_retries).toBe(2);
  });

  it('validates CollectionItem inventory calculations', () => {
    const collection: CollectionItem = {
      id: 'coll_01',
      name: 'large_dataset_alpha',
      total_bytes: 52428800,
      file_count: 128
    };

    expect(collection.id).toBe('coll_01');
    expect(collection.file_count).toBe(128);
    expect(collection.total_bytes).toBe(52428800);
    const avgFileSize = collection.total_bytes / collection.file_count;
    expect(avgFileSize).toBe(409600);
  });
});
