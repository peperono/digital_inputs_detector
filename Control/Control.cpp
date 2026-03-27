#include "Control.h"
#include <cstdio>

// ── Constructor ───────────────────────────────────────────────────────────────

Control::Control() noexcept
    : QP::QActive{Q_STATE_CAST(&Control::initial)}
{}

void Control::setOnEdgeDetected(
    std::function<void(const std::vector<int>&)> cb) noexcept
{
    m_onEdgeDetected = std::move(cb);
}

// ── State: initial ────────────────────────────────────────────────────────────

Q_STATE_DEF(Control, initial) {
    Q_UNUSED_PAR(e);
    subscribe(EDGE_DETECTED_SIG);
    return tran(&Control::running);
}

// ── State: running ────────────────────────────────────────────────────────────

Q_STATE_DEF(Control, running) {
    QP::QState status;

    switch (e->sig) {

        case Q_ENTRY_SIG: {
            status = Q_HANDLED();
            break;
        }

        case EDGE_DETECTED_SIG: {
            auto const* evt = Q_EVT_CAST(EdgeDetectedEvt);
            for (int id : evt->input_ids) {
                std::printf("[Control] flanco detectado en entrada %d\n", id);
            }
            if (m_onEdgeDetected) {
                m_onEdgeDetected(evt->input_ids);
            }
            status = Q_HANDLED();
            break;
        }

        default: {
            status = super(&Control::top);
            break;
        }
    }
    return status;
}
