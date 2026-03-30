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
                std::lock_guard<std::mutex> lk(g_state.mtx);
                g_state.inputs  = evt->inputs;
                g_state.outputs = evt->outputs;
            }
            g_state.push_pending.store(true);
            status = Q_HANDLED();
            break;
        }

        case EDGE_DETECTED_SIG: {
            auto const* evt = Q_EVT_CAST(EdgeDetectedEvt);
            {
                std::lock_guard<std::mutex> lk(g_state.mtx);
                g_state.last_edges = evt->input_ids;
                ++g_state.edge_gen;
                for (int id : evt->input_ids) {
                    ++g_state.edge_counts[id];
                }
            }
            g_state.push_pending.store(true);
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
