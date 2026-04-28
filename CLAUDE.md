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

**Threads:**
- **QV thread (cooperative):** IOReader, DigitalEdgeDetector, Monitor, TestObserver
- **Mongoose thread:** HttpServer, access to SharedState (via mutex)
- **External process:** Browser (HTTP + WebSocket)

**Active Objects (priority order):**

| Priority | AO | Publishes | Subscribes |
|----------|----|-----------|------------|
| 3 | `DigitalEdgeDetector` | `IO_STATE_CHANGED_SIG`, `EDGE_DETECTED_SIG` | `REMOTE_INPUT_SIG`, `RECONFIGURE_SIG` |
| 2 | `Monitor` | — | `IO_STATE_CHANGED_SIG`, `EDGE_DETECTED_SIG` |
| 1 | `TestObserver` | — | both (test mode only) |

**Cross-thread data:** `SharedState se` (defined in `main.cpp`, declared `extern` in `DigitalEdgeDetector/SharedState.h`) is the only shared data between the QV thread and the Mongoose thread. All access is guarded by `se.mtx`. `DigitalEdgeDetector` writes `se.inputs`, `se.outputs`, `se.last_edges`, `se.edge_counts` and sets `se.push_pending = true` directly in the poll handler. The Mongoose thread reads `se` and pushes WebSocket messages when `push_pending` is set.

**IOReader injection:** `DigitalEdgeDetector` accepts an `IOReader = std::function<void(map<int,bool>&, map<int,bool>&)>` at construction. In test mode `makeTestReader()` (`Test/TestController.hpp`) returns a lambda cycling through `TestStep` scenarios. In remote mode `makeRemoteReader()` (`RemoteIO/IOReader_Remot.hpp`) returns a lambda that reads from `RemoteIOState remoteIO` (mutex-protected, written by the Mongoose thread via WebSocket). Future platform implementations: `HWReader` (reads GPIO hardware).

**Event memory:** `IOStateEvt` and `EdgeDetectedEvt` use static (zero-pool) semantics because they hold `std::vector`/`std::unordered_map` which are incompatible with QP memory pools. This is safe under the QV cooperative scheduler. `RemoteInputEvt` and `ReconfigureEvt` use QP memory pools (initialized in `main.cpp`). Max 16 configs per `ReconfigureEvt`.

**Race condition to be aware of:** After a `PUT /configs` HTTP response is sent, the QV poll timer may fire before `RECONFIGURE_SIG` is processed, emitting a WS push with stale IDs. The JS UI guards against this with `expectedInputIds`.

## HTTP endpoints

- `GET /` — serves embedded HTML/JS page
- `GET /configs` — returns `se.configs[]` as JSON array
- `PUT /configs` — replaces full config array, posts `RECONFIGURE_SIG`
- `WebSocket /ws` — pushes `se.inputs/outputs/last_edges/edge_counts`, receives `inputs/outputs`

## Key files

- `signals.h` — all QP signal enums and event struct definitions
- `DigitalEdgeDetector/SharedState.h` — the shared struct between QV and Mongoose threads
- `InputConfig.h` — `InputConfig` struct: `id`, `logic_positive`, `detection_always`, `linked_outputs`
- `Test/TestController.hpp` — TestObserver AO + verifyStep() + makeTestReader() + g_* globals
- `docs/sistema.drawio` — system architecture diagram
- `qp_config.hpp` — QP tunables (`QF_MAX_ACTIVE=32`, `QF_MAX_EPOOL=3`)

El diagrama de referència és `docs/sistema.drawio`. Les convencions visuals (colors, fletxes, etiquetes, estructura dels nodes) estan documentades a [`docs/drawio-conventions.md`](docs/drawio-conventions.md).

## Convencions

- Codi i comentaris en **català**
- Les màquines d'estat segueixen els patrons QP/C++ (HSM jeràrquiques, `Q_TRAN`, `Q_SUPER`, `QM_TRAN`)
- Els events dinàmics s'allotgen via `QF_NEW` i s'alliberen automàticament pel framework
- Vegeu `.claude/plugins/qpcpp-patterns/` per als patrons habituals de QP/C++ al projecte
