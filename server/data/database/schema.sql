-- Table: users
CREATE TABLE IF NOT EXISTS users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    username TEXT NOT NULL UNIQUE,
    password TEXT NOT NULL,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- Seed data: users
INSERT INTO users (username, password) VALUES
('demo', 'demo'),
('alice', 'alice123'),
('bob', 'bob456');

-- Table: sessions
CREATE TABLE IF NOT EXISTS sessions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id INTEGER NOT NULL,
    token TEXT NOT NULL UNIQUE,
    expires_at DATETIME NOT NULL,
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
);

-- Seed data: sessions
INSERT INTO sessions (user_id, token, expires_at) VALUES
(1, 'token_alice_1', datetime('now','+1 day')),
(2, 'token_bob_1', datetime('now','+1 day'));

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

-- Seed data: folders
-- user root folders
INSERT INTO folders (name, parent_id, owner_id, user_root) VALUES
('alice_root', NULL, 1, 1),
('bob_root', NULL, 2, 1);

-- subfolders
INSERT INTO folders (name, parent_id, owner_id, user_root) VALUES
('alice_docs', 1, 1, 0),
('bob_images', 2, 2, 0);

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

-- Seed data: files
INSERT INTO files (name, folder_id, owner_id, storage_hash, size) VALUES
('alice_resume.pdf', 3, 1, 'hash_file1', 102400),
('bob_photo.png', 4, 2, 'hash_file2', 204800);

-- Table: permissions
CREATE TABLE IF NOT EXISTS permissions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    target_type INTEGER NOT NULL CHECK(target_type IN (0,1)), -- 0=file, 1=folder
    target_id INTEGER NOT NULL,
    user_id INTEGER NOT NULL,
    permission INTEGER NOT NULL CHECK(permission IN (0,1,2,3)),
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
);

-- Seed data: permissions
-- alice can read/write bob's folder
INSERT INTO permissions (target_type, target_id, user_id, permission) VALUES
(1, 4, 1, 2),  -- folder permission: alice can write bob_images
(0, 2, 1, 1);  -- file permission: alice can read bob_photo.png
