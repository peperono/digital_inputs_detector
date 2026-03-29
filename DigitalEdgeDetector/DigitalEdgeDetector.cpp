#include "DigitalEdgeDetector.h"

// ── Constructor ───────────────────────────────────────────────────────────────

DigitalEdgeDetector::DigitalEdgeDetector(IOReader reader,
                                         std::uint32_t poll_ticks) noexcept
    : QP::QActive{Q_STATE_CAST(&DigitalEdgeDetector::initial)},
      m_pollTimer{this, EDGE_DETECTOR_POLL_SIG},
      m_reader{std::move(reader)},
      m_pollTicks{poll_ticks},
      m_ioEvt{},
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
    m_pollTimer.armX(m_pollTicks, m_pollTicks);
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

        case EDGE_DETECTOR_POLL_SIG: {
            std::unordered_map<int, bool> inputs;
            std::unordered_map<int, bool> outputs;
            m_reader(inputs, outputs);

            if ((inputs != m_prevInputs) || (outputs != m_prevOutputs)) {
                m_prevInputs  = inputs;
                m_prevOutputs = outputs;

                m_ioEvt.inputs  = inputs;
                m_ioEvt.outputs = outputs;
                PUBLISH(&m_ioEvt, this);

                m_edgeEvt.input_ids.clear();

                for (const auto& cfg : m_configs) {
                    auto it = inputs.find(cfg.id);
                    if (it == inputs.end()) continue;

                    bool current = it->second;
                    bool prev    = m_prevStates.count(cfg.id)
                                       ? m_prevStates.at(cfg.id)
                                       : current; // first scan: no edge

                    bool rising_edge   = !prev && current;
                    bool falling_edge  =  prev && !current;
                    bool edge_detected = cfg.logic_positive ? rising_edge : falling_edge;

                    if (edge_detected && detection_enabled(cfg, outputs)) {
                        m_edgeEvt.input_ids.push_back(cfg.id);
                    }

                    m_prevStates[cfg.id] = current;
                }

                if (!m_edgeEvt.input_ids.empty()) {
                    PUBLISH(&m_edgeEvt, this);
                }
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
