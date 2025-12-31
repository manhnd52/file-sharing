-- Table: users
CREATE TABLE IF NOT EXISTS users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    username TEXT NOT NULL UNIQUE,
    password TEXT NOT NULL,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- Table: sessions
CREATE TABLE IF NOT EXISTS sessions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id INTEGER NOT NULL,
    token TEXT NOT NULL UNIQUE,
    expires_at DATETIME NOT NULL,
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
);

-- Table: folders
CREATE TABLE IF NOT EXISTS folders (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    parent_id INTEGER,
    owner_id INTEGER NOT NULL,
    user_root BOOLEAN NOT NULL DEFAULT 0,
    FOREIGN KEY (parent_id) REFERENCES folders(id) ON DELETE CASCADE,
    FOREIGN KEY (owner_id) REFERENCES users(id) ON DELETE CASCADE
);


-- Table: files
CREATE TABLE IF NOT EXISTS files (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    folder_id INTEGER NOT NULL,
    owner_id INTEGER NOT NULL,
    storage_hash TEXT NOT NULL,
    size INTEGER NOT NULL,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (folder_id) REFERENCES folders(id) ON DELETE CASCADE,
    FOREIGN KEY (owner_id) REFERENCES users(id) ON DELETE CASCADE
);


-- Table: permissions
CREATE TABLE IF NOT EXISTS permissions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    target_type INTEGER NOT NULL CHECK(target_type IN (0,1)), -- 0=file, 1=folder
    target_id INTEGER NOT NULL,
    user_id INTEGER NOT NULL,
    permission INTEGER NOT NULL CHECK(permission IN (0,1,2,3)),
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
);

CREATE TABLE download_sessions (
    session_id            BLOB PRIMARY KEY,
    state                 INTEGER NOT NULL CHECK (state BETWEEN 0 AND 5),
    last_requested_chunk  INTEGER DEFAULT 0 NOT NULL,
    chunk_size            INTEGER NOT NULL,
    total_file_size       INTEGER NOT NULL,
    file_id               INTEGER NOT NULL,
    file_hashcode         TEXT NOT NULL
);

CREATE TABLE upload_sessions (
    id                      INTEGER PRIMARY KEY AUTOINCREMENT,

    -- session identity
    session_id BLOB         NOT NULL UNIQUE,        -- raw UUID (16 bytes)
    uuid_str                TEXT NOT NULL UNIQUE,           -- UUID string (36 chars + null)

    -- upload progress
    last_received_chunk     INTEGER NOT NULL DEFAULT 0,
    chunk_size              INTEGER NOT NULL,
    total_received_size     INTEGER NOT NULL DEFAULT 0,
    expected_file_size      INTEGER NOT NULL,

    -- file info
    parent_folder_id        INTEGER NOT NULL,
    file_name               TEXT NOT NULL,

    -- state
    state                   INTEGER NOT NULL DEFAULT 0,

    -- optional nhưng rất nên có
    created_at              INTEGER NOT NULL DEFAULT (unixepoch()),
    updated_at              INTEGER NOT NULL DEFAULT (unixepoch())
);


INSERT INTO users (username, password) VALUES
('alice', '123'),
('bob',   '123');