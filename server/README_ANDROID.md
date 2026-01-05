# Running File Sharing Server on Android/Termux

## Prerequisites

1. Install Termux from F-Droid (recommended) or Play Store
2. Update packages:
   ```bash
   pkg update && pkg upgrade
   ```

3. Install required packages:
   ```bash
   pkg install clang make sqlite git
   ```

## Building for Android/Termux

1. Clone the repository:
   ```bash
   cd ~/
   git clone <your-repo-url> file-sharing
   cd file-sharing/server
   ```

2. Build the server:
   ```bash
   make clean
   make
   ```

## Running the Server

### Quick Start
```bash
chmod +x run_android.sh
./run_android.sh
```

### Manual Start
```bash
# Create directories
mkdir -p data/database data/storage/tmp

# Initialize database
sqlite3 data/database/file_sharing.db < data/database/schema.sql

# Run server
./server
```

## Common Issues & Solutions

### SIGSEGV Error (Signal 11)
**Cause:** Android/Termux has stricter memory/thread constraints

**Fixed by:**
- ✅ Runtime pthread mutex/cond initialization (not static)
- ✅ Explicit thread stack size (2MB minimum)
- ✅ Proper cleanup functions

### Permission Denied
```bash
chmod +x server
chmod 755 data/database
chmod 644 data/database/file_sharing.db
```

### Port Already in Use
Change port in `src/main.c`:
```c
server_start(5555); // Change to another port like 8080
```

### Out of Memory
Reduce constants in `server.h`:
```c
#define MAX_CONNS 256  // Instead of 1024
#define QUEUE_CAPACITY 256  // Instead of 1024
```

## Android-Specific Limitations

1. **Stack Size**: Default pthread stack is 1MB on Android (vs 8MB on Linux)
   - Solution: Use `pthread_attr_setstacksize()` 

2. **File Descriptors**: Android limits FD count per process
   - Solution: Reduce `MAX_CONNS` if needed

3. **Background Execution**: Android may kill background apps
   - Solution: Use Termux:Boot or keep Termux in foreground

4. **Network Permissions**: Some Android ROMs restrict localhost binding
   - Solution: Bind to `0.0.0.0` instead of `127.0.0.1`

## Testing Connection

From Termux (same device):
```bash
# Install client dependencies
pkg install python

# Test with Python client
cd ../ui
python3 main.py
```

From another device on same network:
```python
# In fs_client.py, change:
DEFAULT_HOST = "192.168.1.xxx"  # Your Android device IP
```

## Debugging

Enable verbose logging:
```bash
# Add to main.c before server_start():
setbuf(stdout, NULL);  // Disable buffering
```

Check thread status:
```bash
# In another Termux session
ps -T -p $(pidof server)
```

## Performance Tips

1. Use local storage (not SD card) for `data/` folder
2. Reduce `MAX_PAYLOAD` if memory constrained
3. Consider using `nice -n -5 ./server` for better priority

## Stopping the Server

- Press `Ctrl+C` in Termux
- Or: `pkill -SIGTERM server`

## Clean Reinstall

```bash
make clean
rm -rf data/database/*.db
rm -rf data/storage/*
make
./run_android.sh
```
