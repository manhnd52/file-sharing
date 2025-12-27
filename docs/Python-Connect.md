# Python Bindings for Connect

The Python extension wraps a synchronous `Connect`: every `send_*` call blocks
until the server returns the matching `RESPOND`/`DATA` frame for that request.

## Build

```
cd python
python3 setup.py build
```

The extension compiles `connect.c`/`frame.c` with `connect_module.c`. Install the
Python development headers (e.g. `python3-dev`) so `Python.h` is available.

## API

Create a `Connect` object and call the blocking helpers:

```python
from file_sharing_connect import Connect

conn = Connect("127.0.0.1", 5555)
```

### send_cmd(payload)

`payload` accepts `str` or `bytes`. The method waits for the response, converts it
to a dictionary (`msg_type`, `request_id`, `payload`, `payload_text`, `status`, …)
and returns it.

```python
resp = conn.send_cmd('{"cmd":"LIST"}')
print("Got response", resp.get("payload_text"))
```

### send_data(session_id, chunk_index, data)

Send a `DATA` frame; `session_id` must be 16 bytes and `data` must fit within
`MAX_PAYLOAD`. The returned dict follows the same structure as `send_cmd`.

### Lifecycle helpers

Call `conn.close()` or use `with Connect(...) as conn:` to tear down the socket.
