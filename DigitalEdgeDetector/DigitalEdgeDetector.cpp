#include "DigitalEdgeDetector.h"
#include "SharedState.h"
#include <mutex>

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

                {
                    std::lock_guard<std::mutex> lk(se.mtx);
                    se.inputs  = inputs;
                    se.outputs = outputs;
                    if (!m_edgeEvt.input_ids.empty()) {
                        se.last_edges = m_edgeEvt.input_ids;
                        for (int id : m_edgeEvt.input_ids)
                            ++se.edge_counts[id];
                    } else {
                        se.last_edges.clear();
                    }
                }
                se.push_pending.store(true);
            }

            status = Q_HANDLED();
            break;
        }

        case RECONFIGURE_SIG: {
            auto const* evt = Q_EVT_CAST(ReconfigureEvt);
            std::vector<InputConfig> newConfigs;
            for (int i = 0; i < evt->n_configs; ++i) {
                auto const& e = evt->entries[i];
                InputConfig cfg;
                cfg.id               = e.id;
                cfg.logic_positive   = e.logic_positive;
                cfg.detection_always = e.detection_always;
                for (int j = 0; j < e.n_linked; ++j)
                    cfg.linked_outputs.push_back(e.linked_outputs[j]);
                newConfigs.push_back(std::move(cfg));
            }
            configure(newConfigs);
            m_prevInputs.clear();
            m_prevOutputs.clear();
            {
                std::lock_guard<std::mutex> lk(se.mtx);
                se.configs = m_configs;
                se.inputs.clear();
                se.outputs.clear();
                se.edge_counts.clear();
                se.last_edges.clear();
                for (const auto& cfg : m_configs) {
                    se.inputs[cfg.id] = false;
                    for (int out_id : cfg.linked_outputs)
                        se.outputs[out_id] = false;
                }
            }
            se.push_pending.store(true);
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
