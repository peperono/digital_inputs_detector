#pragma once
#include "../qpcpp/include/qpcpp.hpp"
#include "../signals.h"
#include "InputConfig.h"
#include <functional>
#include <vector>
#include <unordered_map>
#include <cstdint>

using IOReader = std::function<void(
    std::unordered_map<int, bool>& inputs,
    std::unordered_map<int, bool>& outputs)>;

// ── DigitalEdgeDetector ───────────────────────────────────────────────────────
// Active Object that periodically polls IO via an IOReader callback,
// detects configured rising/falling edges, and publishes EDGE_DETECTED_SIG.
class DigitalEdgeDetector : public QP::QActive {
public:
    explicit DigitalEdgeDetector(IOReader reader, std::uint32_t poll_ticks) noexcept;

    // Must be called before starting the AO.
    void configure(const std::vector<InputConfig>& configs);
    void setRemoteMode(bool remote) noexcept { m_remoteMode = remote; }

private:
    QP::QTimeEvt  m_pollTimer;
    IOReader      m_reader;
    std::uint32_t m_pollTicks;

    std::vector<InputConfig>      m_configs;
    std::unordered_map<int, bool> m_prevStates;
    std::unordered_map<int, bool> m_prevInputs;
    std::unordered_map<int, bool> m_prevOutputs;
    bool                          m_remoteMode{false};
    std::unordered_map<int, bool> m_remoteInputs;
    std::unordered_map<int, bool> m_remoteOutputs;
    IOStateEvt                    m_ioEvt;
    EdgeDetectedEvt               m_edgeEvt;

    bool detection_enabled(const InputConfig& cfg,
                            const std::unordered_map<int, bool>& outputs) const;

    Q_STATE_DECL(initial);
    Q_STATE_DECL(operating);
};
