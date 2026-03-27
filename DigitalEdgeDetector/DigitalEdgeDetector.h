#pragma once
#include "../qpcpp/include/qpcpp.hpp"
#include "../signals.h"
#include <vector>
#include <unordered_map>

struct InputConfig {
    int  id;
    bool logic_positive;   // true  → rising edge (false→true) triggers pulse
    bool detection_always; // true  → always active; false → only when a linked output is ON
    std::vector<int> linked_outputs;
};

// ── DigitalEdgeDetector ───────────────────────────────────────────────────────
// Active Object that subscribes to IO_STATE_CHANGED_SIG (published by
// IOStateMonitor) and detects configured rising/falling edges.
// Publishes EdgeDetectedEvt on EDGE_DETECTED_SIG when one or more edges fire.
class DigitalEdgeDetector : public QP::QActive {
public:
    explicit DigitalEdgeDetector() noexcept;

    // Must be called before starting the AO (or from initial pseudo-state).
    void configure(const std::vector<InputConfig>& configs);

private:
    std::vector<InputConfig>      m_configs;
    std::unordered_map<int, bool> m_prevStates;
    EdgeDetectedEvt               m_edgeEvt; // reused static event

    bool detection_enabled(const InputConfig& cfg,
                            const std::unordered_map<int, bool>& outputs) const;

    Q_STATE_DECL(initial);
    Q_STATE_DECL(operating);
};
