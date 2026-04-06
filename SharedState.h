#pragma once
#include "DigitalEdgeDetector/InputConfig.h"
#include <atomic>
#include <mutex>
#include <string>
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
    bool                          remote_mode{false};  // true when using RemoteReader
    std::atomic<bool>             push_pending{false}; // set by WsPublisher, cleared by Mongoose
    std::vector<InputConfig>      configs;             // written at startup and on reconfigure
    std::string                   test_log;            // test mode status (empty in remote mode)
};

extern SharedState se;
