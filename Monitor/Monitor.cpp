#include "Monitor.h"
#include "../SharedState.h"
#include <cstdio>

// ── Constructor ───────────────────────────────────────────────────────────────

Monitor::Monitor() noexcept
    : QP::QActive{Q_STATE_CAST(&Monitor::initial)}
{}

// ── State: initial ────────────────────────────────────────────────────────────

Q_STATE_DEF(Monitor, initial) {
    Q_UNUSED_PAR(e);
    subscribe(IO_STATE_CHANGED_SIG);
    subscribe(EDGE_DETECTED_SIG);
    return tran(&Monitor::running);
}

// ── State: running ────────────────────────────────────────────────────────────

Q_STATE_DEF(Monitor, running) {
    QP::QState status;

    switch (e->sig) {

        case Q_ENTRY_SIG: {
            status = Q_HANDLED();
            break;
        }

        case IO_STATE_CHANGED_SIG: {
            auto const* evt = Q_EVT_CAST(IOStateEvt);
            for (auto const& [id, state] : evt->inputs) {
                std::printf("[Monitor] entrada %d -> %s\n", id, state ? "ON" : "OFF");
            }
            for (auto const& [id, state] : evt->outputs) {
                std::printf("[Monitor] salida  %d -> %s\n", id, state ? "ON" : "OFF");
            }
            {
                std::lock_guard<std::mutex> lk(g_state.mtx);
                g_state.inputs  = evt->inputs;
                g_state.outputs = evt->outputs;
            }
            status = Q_HANDLED();
            break;
        }

        case EDGE_DETECTED_SIG: {
            auto const* evt = Q_EVT_CAST(EdgeDetectedEvt);
            for (int id : evt->input_ids) {
                std::printf("[Monitor] flanco detectado en entrada %d\n", id);
            }
            {
                std::lock_guard<std::mutex> lk(g_state.mtx);
                g_state.last_edges = evt->input_ids;
                ++g_state.edge_gen;
                for (int id : evt->input_ids) {
                    ++g_state.edge_counts[id];
                }
            }
            status = Q_HANDLED();
            break;
        }

        default: {
            status = super(&Monitor::top);
            break;
        }
    }
    return status;
}
