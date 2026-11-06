// API contract types derived from OpenAPI /api/v1 specification

export interface HealthResponse {
  status: string;
  product: string;
  version: string;
}

export interface EngineStatusResponse {
  product: string;
  version: string;
  total_bytes_transferred: number;
  total_files_completed: number;
  total_retries: number;
}

export interface CollectionItem {
  id: string;
  name: string;
  total_bytes: number;
  file_count: number;
}
