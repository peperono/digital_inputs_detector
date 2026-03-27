#include "DigitalEdgeDetector.h"

// ── Constructor ───────────────────────────────────────────────────────────────

DigitalEdgeDetector::DigitalEdgeDetector() noexcept
    : QP::QActive{Q_STATE_CAST(&DigitalEdgeDetector::initial)},
      m_edgeEvt{}
{}

// ── Public API ────────────────────────────────────────────────────────────────

void DigitalEdgeDetector::configure(const std::vector<InputConfig>& configs) {
    m_configs = configs;
    m_prevStates.clear();
}

// ── State: initial (pseudo-state) ─────────────────────────────────────────────

Q_STATE_DEF(DigitalEdgeDetector, initial) {
    Q_UNUSED_PAR(e);
    // Subscribe to the event published by IOStateMonitor.
    subscribe(IO_STATE_CHANGED_SIG);
    return tran(&DigitalEdgeDetector::operating);
}

// ── State: operating ──────────────────────────────────────────────────────────

Q_STATE_DEF(DigitalEdgeDetector, operating) {
    QP::QState status;

    switch (e->sig) {

        case Q_ENTRY_SIG: {
            status = Q_HANDLED();
            break;
        }

        case IO_STATE_CHANGED_SIG: {
            auto const* ioEvt = Q_EVT_CAST(IOStateEvt);

            m_edgeEvt.input_ids.clear();

            for (const auto& cfg : m_configs) {
                auto it = ioEvt->inputs.find(cfg.id);
                if (it == ioEvt->inputs.end()) continue;

                bool current = it->second;
                bool prev    = m_prevStates.count(cfg.id)
                                   ? m_prevStates.at(cfg.id)
                                   : current; // first scan: no edge

                bool rising_edge   = !prev && current;
                bool falling_edge  =  prev && !current;
                bool edge_detected = cfg.logic_positive ? rising_edge : falling_edge;

                if (edge_detected && detection_enabled(cfg, ioEvt->outputs)) {
                    m_edgeEvt.input_ids.push_back(cfg.id);
                }

                m_prevStates[cfg.id] = current;
            }

            if (!m_edgeEvt.input_ids.empty()) {
                PUBLISH(&m_edgeEvt, this);
            }

            status = Q_HANDLED();
            break;
        }

        default: {
            status = super(&DigitalEdgeDetector::top);
            break;
        }
    }
    return status;
}

// ── Private helpers ───────────────────────────────────────────────────────────

bool DigitalEdgeDetector::detection_enabled(
    const InputConfig& cfg,
    const std::unordered_map<int, bool>& outputs) const
{
    if (cfg.detection_always) return true;

    for (int out_id : cfg.linked_outputs) {
        auto it = outputs.find(out_id);
        if (it != outputs.end() && it->second) return true;
    }
    return false;
}
