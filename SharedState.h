#pragma once
#include <mutex>
#include <unordered_map>
#include <vector>

// ── SharedState ───────────────────────────────────────────────────────────────
// Shared between the QV thread (Monitor AO writes) and the Mongoose HTTP thread
// (HttpServer reads). Access must be guarded by mtx.

struct SharedState {
    std::mutex                    mtx;
    std::unordered_map<int, bool> inputs;
    std::unordered_map<int, bool> outputs;
    std::vector<int>              last_edges;
};

extern SharedState g_state;
