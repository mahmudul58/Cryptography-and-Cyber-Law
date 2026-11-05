# OSI 7-Layer Communication Simulation

A small project that shows how data travels through the 7 layers of the OSI model.

- A **C client** wraps a message layer by layer (encapsulation) and sends it over a **real TCP socket**.
- A **C server** receives it and unwraps it layer by layer (decapsulation).
- A **Python app** runs both and prints the zig-zag flow, **two layers at a time**:
  sender goes down L7 → L1, the data crosses the socket, receiver goes up L1 → L7.

```
SENDER (client)                                  RECEIVER (server)
L7 Application   ─┐                              ┌─▶ L7 Application
L6 Presentation   │                              │   L6 Presentation
L5 Session        │                              │   L5 Session
L4 Transport      │                              │   L4 Transport
L3 Network        │                              │   L3 Network
L2 Data Link      │                              │   L2 Data Link
L1 Physical  ─────┴══ TCP socket 127.0.0.1 ══════┘   L1 Physical
```

## What each layer does

| Layer | On send (adds) | On receive (removes) |
|-------|----------------|----------------------|
| 7 Application | `APP` header with message length | `APP` header |
| 6 Presentation | `PRS` header, data encoded (shift + hex, a toy example, not real encryption) | header, data decoded |
| 5 Session | `SES` header with session id | `SES` header |
| 4 Transport | `TCP` header with ports | `TCP` header |
| 3 Network | `IP` header with source and destination | `IP` header |
| 2 Data Link | `ETH` header with MAC addresses and a checksum | header, checksum verified |
| 1 Physical | frame turned into bytes/bits and sent on the socket | frame received from the socket |

## Files

| File | Purpose |
|------|---------|
| `layers.c`, `layers.h` | `encapsulate()` (L7 → L1) and `decapsulate()` (L1 → L7) |
| `client.c` | Encapsulates the message and sends it over TCP |
| `server.c` | Receives the frame, decapsulates it, replies with an ACK |
| `netutil.h` | Length-prefixed send/receive helpers for TCP |
| `visualizer.py` | Runs everything and prints the zig-zag flow |
| `Makefile` | Builds `server` and `client` |
