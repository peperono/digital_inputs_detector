#pragma once
#include "../qpcpp/include/qpcpp.hpp"
#include "../signals.h"

// ── Monitor ───────────────────────────────────────────────────────────────────
// Active Object que suscribe a IO_STATE_CHANGED_SIG y EDGE_DETECTED_SIG.
class Monitor : public QP::QActive {
public:
    explicit Monitor() noexcept;

private:
    Q_STATE_DECL(initial);
    Q_STATE_DECL(running);
};
