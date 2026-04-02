# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build

```bash
bash build.sh
```

Output: `build/app.exe`. The script compiles `mongoose/mongoose.c` with `gcc` then links everything with `g++ -std=c++17 -O1 -Wall -static -lwinmm -lws2_32`.

**Important:** The HTML/JS UI is embedded as a string constant in `HttpServer/HttpServer.cpp`. Any JS/HTML change requires a full recompile, app restart, and hard browser refresh (Ctrl+Shift+R).

## Run

```bash
./build/app.exe
# 1 → Test mode (automated IO sequence, prints OK/FALLO per step)
# 2 → Remote mode (web UI at http://localhost:8080)
```

## Architecture

**Framework:** QP/C++ with the QV cooperative scheduler (single thread). A separate Mongoose thread handles HTTP/WebSocket I/O.

**Active Objects (priority order):**

| Priority | AO | Publishes | Subscribes |
|----------|----|-----------|------------|
| 4 | `DigitalEdgeDetector` | `IO_STATE_CHANGED_SIG`, `EDGE_DETECTED_SIG` | `REMOTE_INPUT_SIG`, `RECONFIGURE_SIG` |
| 3 | `Monitor` | — | `IO_STATE_CHANGED_SIG`, `EDGE_DETECTED_SIG` |
| 2 | `WsPublisher` | — | `IO_STATE_CHANGED_SIG`, `EDGE_DETECTED_SIG` |
| 1 | `TestObserver` | — | both (test mode only) |

**Cross-thread data:** `SharedState se` (defined in `main.cpp`, declared `extern` in `SharedState.h`) is the only shared data between the QV thread and the Mongoose thread. All access is guarded by `se.mtx`. When `WsPublisher` updates `se`, it sets `se.push_pending = true` to signal the Mongoose thread to push a WebSocket message.

**IOReader injection:** `DigitalEdgeDetector` accepts an `IOReader = std::function<void(map<int,bool>&, map<int,bool>&)>` at construction. In test mode `makeTestReader()` returns a lambda cycling through `TestStep` scenarios. In remote mode the reader is empty (`IOReader{}`) and state comes from `REMOTE_INPUT_SIG` events published by the Mongoose thread.

**Event memory:** `IOStateEvt` and `EdgeDetectedEvt` use static (zero-pool) semantics because they hold `std::vector`/`std::unordered_map` which are incompatible with QP memory pools. This is safe under the QV cooperative scheduler. `RemoteInputEvt` and `ReconfigureEvt` use QP memory pools (initialized in `main.cpp`).

**Race condition to be aware of:** After a `PUT /config` HTTP response is sent, the QV poll timer may fire before `RECONFIGURE_SIG` is processed, emitting a WS push with stale IDs. The JS UI guards against this with `expectedInputIds`.

## Key files

- `signals.h` — all QP signal enums and event struct definitions
- `SharedState.h` — the shared struct between QV and Mongoose threads
- `InputConfig.h` — `InputConfig` struct: `id`, `logic_positive`, `detection_always`, `linked_outputs`
- `TestIOReader.hpp` — test steps + `TestObserver` AO + `verifyStep()` + `makeTestReader()`
- `docs/openapi.yaml` — REST API spec (GET `/state`, `/config`; PUT/POST/DELETE `/config/{id}`)
- `qp_config.hpp` — QP tunables (`QF_MAX_ACTIVE=32`, `QF_MAX_EPOOL=3`)
