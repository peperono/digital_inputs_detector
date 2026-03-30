#pragma once
#include "../qpcpp/include/qpcpp.hpp"
#include "../signals.h"

// ── WsPublisher ───────────────────────────────────────────────────────────────
// Active Object that subscribes to IO_STATE_CHANGED_SIG and EDGE_DETECTED_SIG
// published by DigitalEdgeDetector. On each event it updates SharedState and
// sets g_state.push_pending so the Mongoose thread sends a WebSocket push
// without waiting for the next 100 ms poll cycle.
class WsPublisher : public QP::QActive {
public:
    explicit WsPublisher() noexcept;

private:
    Q_STATE_DECL(initial);
    Q_STATE_DECL(running);
};
