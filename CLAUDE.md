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

**IOReader injection:** `DigitalEdgeDetector` accepts an `IOReader = std::function<void(map<int,bool>&, map<int,bool>&)>` at construction. In test mode `makeTestReader()` returns a lambda cycling through `TestStep` scenarios. In remote mode the reader is empty (`IOReader{}`) and state comes from `REMOTE_INPUT_SIG` events posted by the Mongoose thread. Platform implementations: `IOReader_ESP32` (reads GPIO hardware), `IOReader_Win32` (simulates IO).

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

El diagrama de referència és `docs/ControlRemot.drawio`. Segueix aquest esquema visual:

### Codi de colors

| Color | Hex fill / stroke | Representa |
|---|---|---|
| Blau | `#dae8fc` / `#6c8ebf` | Active Object (fil QP, `QP::QActive`) |
| Verd | `#d5e8d4` / `#82b366` | Event QP (entrada o sortida, `QP::QEvt`) |
| Groc | `#fff2cc` / `#d6b656` | Memòria compartida entre fils (`SharedState`, mutex) |
| Rosa | `#ffcccc` / `#36393d` | Thread Mongoose (`HttpServer`) |
| Blanc | `#ffffff` / `#666666` | Actor extern (Browser, REST, WebSocket) |

### Estructura d'un Active Object

Cada AO és un rectangle blau amb `verticalAlign=top`, títol en negreta `NomComponent (QP::QActive)`. A l'interior, rectangles fills independents:
- Esquerra: events d'entrada (verd), un per signal
- Dreta: events de sortida publicats (verd) i variables de `SharedState` (groc)

### Etiquetes de les arestes

| Etiqueta | Significat |
|---|---|
| `publish` | `QF_PUBLISH` — bus QP, qualsevol subscrit el rep |
| `POST` | `QActive::POST` — directe a un AO, thread-safe |
| `push` | HttpServer llegeix `SharedState` i envia per WebSocket |
| `write` | Escriptura a `SharedState` sota mutex |

### Convencions generals

- Tot el diagrama va dins un `shape=umlFrame` amb títol `NomProjecte / Data`.
- La llegenda de colors és un node `text` dins el frame, cantonada inferior dreta.
- Format del nom d'event als nodes: `NomEvt\n(SIGNAL_NAME, mecanisme)` — dues línies, `fontSize=8`.
- Arestes ortogonals (`edgeStyle=orthogonalEdgeStyle`) amb punts de routing explícits quan hi ha creuaments.

## Convencions

- Codi i comentaris en **català**
- Les màquines d'estat segueixen els patrons QP/C++ (HSM jeràrquiques, `Q_TRAN`, `Q_SUPER`, `QM_TRAN`)
- Els events dinàmics s'allotgen via `QF_NEW` i s'alliberen automàticament pel framework
- Vegeu `.claude/plugins/qpcpp-patterns/` per als patrons habituals de QP/C++ al projecte
