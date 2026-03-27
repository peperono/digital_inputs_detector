#include "IOStateMonitor.h"

// ── Constructor ───────────────────────────────────────────────────────────────

IOStateMonitor::IOStateMonitor(IOReader reader, std::uint32_t poll_ticks) noexcept
    : QP::QActive{Q_STATE_CAST(&IOStateMonitor::initial)},
      m_pollTimer{this, IO_MONITOR_POLL_SIG},
      m_reader{std::move(reader)},
      m_pollTicks{poll_ticks},
      m_evt{}
{}

// ── State: initial (pseudo-state) ─────────────────────────────────────────────

Q_STATE_DEF(IOStateMonitor, initial) {
    Q_UNUSED_PAR(e);
    // Arm the periodic timer (first tick after m_pollTicks, then every m_pollTicks).
    m_pollTimer.armX(m_pollTicks, m_pollTicks);
    return tran(&IOStateMonitor::monitoring);
}

// ── State: monitoring ─────────────────────────────────────────────────────────

Q_STATE_DEF(IOStateMonitor, monitoring) {
    QP::QState status;

    switch (e->sig) {

        case Q_ENTRY_SIG: {
            status = Q_HANDLED();
            break;
        }

        case IO_MONITOR_POLL_SIG: {
            // Read current IO snapshot via the injected callback.
            std::unordered_map<int, bool> inputs;
            std::unordered_map<int, bool> outputs;
            m_reader(inputs, outputs);

            // Publish only when something actually changed.
            if ((inputs != m_prevInputs) || (outputs != m_prevOutputs)) {
                m_prevInputs  = inputs;
                m_prevOutputs = outputs;

                m_evt.inputs  = inputs;
                m_evt.outputs = outputs;
                PUBLISH(&m_evt, this);
            }
            status = Q_HANDLED();
            break;
        }

        default: {
            status = super(&IOStateMonitor::top);
            break;
        }
    }
    return status;
}
