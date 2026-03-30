#pragma once
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
    std::uint32_t                 edge_gen{0};    // incremented on each EDGE_DETECTED_SIG
    std::unordered_map<int, int>  edge_counts;    // cumulative edge count per input id
    std::vector<int>              configured_inputs;   // set once at startup
    std::vector<int>              configured_outputs;  // set once at startup
    bool                          remote_mode{false};  // true when using RemoteReader
};

extern SharedState g_state;
