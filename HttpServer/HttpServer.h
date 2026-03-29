#pragma once
#include <cstdint>

// ── HttpServer ────────────────────────────────────────────────────────────────
// Runs a Mongoose HTTP server in a dedicated thread.
// Exposes read-only endpoints backed by g_state (SharedState.h).
//
//   GET /state  →  JSON with current inputs and outputs
//   GET /edges  →  JSON with last detected edge IDs

namespace HttpServer {
    void start(uint16_t port);
    void stop();
}
