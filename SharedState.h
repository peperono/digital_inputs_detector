#pragma once
#include "DigitalEdgeDetector/DigitalEdgeDetector.h"
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <cstdint>

// ── SharedState ───────────────────────────────────────────────────────────────
// Shared between the QV thread (Monitor AO writes) and the Mongoose HTTP thread
// (HttpServer reads). Access must be guarded by mtx.

struct SharedState {
    std::mutex                    mtx;
    std::unordered_map<int, bool> inputs;
    std::unordered_map<int, bool> outputs;
    std::vector<int>              last_edges;
    std::unordered_map<int, int>  edge_counts;    // cumulative edge count per input id
    std::vector<int>              configured_inputs;   // set once at startup
    std::vector<int>              configured_outputs;  // set once at startup
    bool                          remote_mode{false};  // true when using RemoteReader
    std::atomic<bool>             push_pending{false}; // set by WsPublisher, cleared by Mongoose
    std::vector<InputConfig>      configs;             // written at startup and on reconfigure
};

extern SharedState g_state;
