#include "WsPublisher.h"
#include "../SharedState.h"
#include <mutex>

// ── Constructor ───────────────────────────────────────────────────────────────

WsPublisher::WsPublisher() noexcept
    : QP::QActive{Q_STATE_CAST(&WsPublisher::initial)}
{}

// ── State: initial ────────────────────────────────────────────────────────────

Q_STATE_DEF(WsPublisher, initial) {
    Q_UNUSED_PAR(e);
    subscribe(IO_STATE_CHANGED_SIG);
    subscribe(EDGE_DETECTED_SIG);
    return tran(&WsPublisher::running);
}

// ── State: running ────────────────────────────────────────────────────────────

Q_STATE_DEF(WsPublisher, running) {
    QP::QState status;

    switch (e->sig) {

        case Q_ENTRY_SIG: {
            status = Q_HANDLED();
            break;
        }

        case IO_STATE_CHANGED_SIG: {
            auto const* evt = Q_EVT_CAST(IOStateEvt);
            {
                std::lock_guard<std::mutex> lk(se.mtx);
                se.inputs  = evt->inputs;
                se.outputs = evt->outputs;
            }
            se.push_pending.store(true);
            status = Q_HANDLED();
            break;
        }

        case EDGE_DETECTED_SIG: {
            auto const* evt = Q_EVT_CAST(EdgeDetectedEvt);
            {
                std::lock_guard<std::mutex> lk(se.mtx);
                se.last_edges = evt->input_ids;
                for (int id : evt->input_ids) {
                    ++se.edge_counts[id];
                }
            }
            se.push_pending.store(true);
            status = Q_HANDLED();
            break;
        }

        default: {
            status = super(&WsPublisher::top);
            break;
        }
    }
    return status;
}
