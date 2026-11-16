-- Anaiero interest leads, private in Cloudflare D1, not exposed via public API.
-- Rows are NOT a mailing platform; consent and verification lifecycle stays explicit.
CREATE TABLE IF NOT EXISTS interest_leads (
  id TEXT PRIMARY KEY NOT NULL,
  email TEXT NOT NULL COLLATE NOCASE UNIQUE,
  interest_area TEXT NOT NULL CHECK (interest_area IN ('research','engineering','collaboration','studio','general')),
  consent_version TEXT NOT NULL,
  consent_at TEXT NOT NULL,
  origin_path TEXT NOT NULL,
  status TEXT NOT NULL DEFAULT 'awaiting_confirmation',
  created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);
CREATE INDEX IF NOT EXISTS idx_interest_status_created ON interest_leads(status, created_at);
