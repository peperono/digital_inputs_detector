#pragma once
#include "qpcpp/include/qpcpp.hpp"
#include <unordered_map>
#include <vector>

// ── Signals ───────────────────────────────────────────────────────────────────
// Q_USER_SIG = 4, reserved: Q_EMPTY_SIG=0, Q_ENTRY_SIG=1, Q_EXIT_SIG=2, Q_INIT_SIG=3

enum Signals : QP::QSignal {
    IO_STATE_CHANGED_SIG = QP::Q_USER_SIG, // published by IOStateMonitor
    EDGE_DETECTED_SIG,                      // published by DigitalEdgeDetector
    IO_MONITOR_POLL_SIG,                    // internal time-event of IOStateMonitor
    MAX_SIG
};

// ── Events ────────────────────────────────────────────────────────────────────

// Published by IOStateMonitor whenever the physical IO state changes.
// NOTE: uses static event semantics (poolNum_=0) because std::unordered_map
//       is non-trivially destructible and cannot live in a QP memory pool.
//       Safe for the single-threaded win32-qv port (run-to-completion).
struct IOStateEvt : public QP::QEvt {
    std::unordered_map<int, bool> inputs;
    std::unordered_map<int, bool> outputs;

    explicit IOStateEvt() noexcept
        : QP::QEvt{IO_STATE_CHANGED_SIG}
    {}
};

// Published by DigitalEdgeDetector when one or more configured edges fire
// in a single scan cycle.
// Same static event semantics as IOStateEvt.
struct EdgeDetectedEvt : public QP::QEvt {
    std::vector<int> input_ids; // IDs of all inputs that triggered this cycle

    explicit EdgeDetectedEvt() noexcept
        : QP::QEvt{EDGE_DETECTED_SIG}
    {}
};
