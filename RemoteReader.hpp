#pragma once
#include "DigitalEdgeDetector/DigitalEdgeDetector.h"
#include <mutex>

// ── RemoteReaderState ─────────────────────────────────────────────────────────
// Written by the Mongoose HTTP thread (incoming WebSocket messages).
// Read by the IOReader lambda running in the QV thread.
// Access must be guarded by mtx.

struct RemoteReaderState {
    std::mutex                    mtx;
    std::unordered_map<int, bool> inputs;
    std::unordered_map<int, bool> outputs;
};

extern RemoteReaderState g_remoteState;

// ── makeRemoteReader ──────────────────────────────────────────────────────────
// Returns an IOReader that returns whatever the browser last sent.

inline IOReader makeRemoteReader() {
    return [](std::unordered_map<int, bool>& inputs,
              std::unordered_map<int, bool>& outputs) {
        std::lock_guard<std::mutex> lk(g_remoteState.mtx);
        inputs  = g_remoteState.inputs;
        outputs = g_remoteState.outputs;
    };
}
