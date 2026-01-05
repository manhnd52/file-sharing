#!/data/data/com.termux/files/usr/bin/bash
# Script to run file-sharing server on Termux/Android

set -e

echo "=== File Sharing Server for Android/Termux ==="

# Check if running on Termux
if [ ! -d "/data/data/com.termux" ]; then
    echo "Warning: Not running on Termux, using standard paths"
fi

# Create necessary directories with proper permissions
mkdir -p data/database
mkdir -p data/storage/tmp
chmod 755 data
chmod 755 data/database
chmod 755 data/storage
chmod 755 data/storage/tmp

# Initialize database if not exists
if [ ! -f "data/database/file_sharing.db" ]; then
    echo "Initializing database..."
    sqlite3 data/database/file_sharing.db < data/database/schema.sql
    chmod 644 data/database/file_sharing.db
fi

# Set thread stack size for Android (prevent SIGSEGV)
export PTHREAD_STACK_SIZE=2097152  # 2MB

# Disable address space randomization if possible (may help with linker issues)
# Note: This may require root on some Android versions
# setprop persist.sys.vm.randomize_va_space 0 2>/dev/null || true

echo "Starting server on port 5555..."
echo "Press Ctrl+C to stop"
echo ""

# Run with increased limits
ulimit -s 8192 2>/dev/null || true  # Set stack size to 8MB if possible

# Run server
./server

echo ""
echo "Server stopped."
