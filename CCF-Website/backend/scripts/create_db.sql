-- Create team_members table
CREATE TABLE IF NOT EXISTS team_members (
    id SERIAL PRIMARY KEY,
    name VARCHAR(255) NOT NULL,
    title VARCHAR(255),
    department VARCHAR(100) NOT NULL,
    description TEXT,
    bio TEXT,
    linkedin_url VARCHAR(500),
    twitter_url VARCHAR(500),
    email VARCHAR(255),
    image_url VARCHAR(500),
    sort_order INTEGER DEFAULT 0,
    is_active BOOLEAN DEFAULT TRUE,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

-- Create indexes for better query performance
CREATE INDEX IF NOT EXISTS idx_team_members_department ON team_members(department);
CREATE INDEX IF NOT EXISTS idx_team_members_name ON team_members(name);
CREATE INDEX IF NOT EXISTS idx_team_members_active ON team_members(is_active);
CREATE INDEX IF NOT EXISTS idx_team_members_sort ON team_members(sort_order);

-- Create updated_at trigger to auto-update on row modification
CREATE OR REPLACE FUNCTION update_updated_at_column()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = NOW();
    RETURN NEW;
END;
$$ language 'plpgsql';

DROP TRIGGER IF EXISTS update_team_members_updated_at ON team_members;
CREATE TRIGGER update_team_members_updated_at
    BEFORE UPDATE ON team_members
    FOR EACH ROW
    EXECUTE FUNCTION update_updated_at_column();

-- Grant permissions to the owner
GRANT ALL PRIVILEGES ON TABLE team_members TO chardapple;
GRANT ALL PRIVILEGES ON SEQUENCE team_members_id_seq TO chardapple;

-- Sessions for the internal members-area auth (Google OAuth).
-- Opaque server-side sessions so logout / revocation works; the browser cookie
-- carries only the random token, never user data.
CREATE TABLE IF NOT EXISTS sessions (
    token       VARCHAR(64) PRIMARY KEY,
    email       VARCHAR(255) NOT NULL,
    name        VARCHAR(255),
    picture     VARCHAR(1000),
    created_at  TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    expires_at  TIMESTAMP WITH TIME ZONE NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_sessions_expires ON sessions(expires_at);

GRANT ALL PRIVILEGES ON TABLE sessions TO chardapple;
