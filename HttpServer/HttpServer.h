#pragma once
#include "../qpcpp/include/qpcpp.hpp"
#include <cstdint>

// ── HttpServer ────────────────────────────────────────────────────────────────
// Runs a Mongoose HTTP server in a dedicated thread.
//
//   GET  /                        → HTML monitor page
//   GET  /state                   → JSON with current inputs and outputs
//   GET  /config                  → JSON array of all InputConfigs
//   POST /config                  → add InputConfig, reconfigure detector
//   PUT  /config/{id}             → replace InputConfig, reconfigure detector
//   DELETE /config/{id}           → remove InputConfig, reconfigure detector
//   WS   /ws                      → WebSocket real-time state stream

namespace HttpServer {
    void start(uint16_t port, QP::QActive* edgeDetector);
    void stop();
}
