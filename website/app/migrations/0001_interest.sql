-- Anaiero.io research interest registrations. Private Cloudflare D1 resource.
-- The public Worker does not expose list, export or deletion APIs.
-- Status "awaiting_confirmation" is not a verified subscription.
CREATE TABLE IF NOT EXISTS interest_leads (
  id TEXT PRIMARY KEY NOT NULL,
  email TEXT NOT NULL COLLATE NOCASE UNIQUE,
  interest_area TEXT NOT NULL CHECK (
    interest_area IN ('research','engineering','collaboration','studio','general')
  ),
  consent_version TEXT NOT NULL,
  consent_at TEXT NOT NULL,
  source_path TEXT NOT NULL,
  status TEXT NOT NULL DEFAULT 'awaiting_confirmation' CHECK (
    status IN ('awaiting_confirmation','confirmed','unsubscribed')
  ),
  created_at TEXT NOT NULL
);
CREATE INDEX IF NOT EXISTS idx_interest_status ON interest_leads(status, created_at);
