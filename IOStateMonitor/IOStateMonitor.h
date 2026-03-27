#pragma once
#include "../qpcpp/include/qpcpp.hpp"
#include "../signals.h"
#include <functional>
#include <unordered_map>
#include <cstdint>

// Callback that reads the current physical IO state.
// The implementor fills 'inputs' and 'outputs' with { id -> state } pairs.
using IOReader = std::function<void(
    std::unordered_map<int, bool>& inputs,
    std::unordered_map<int, bool>& outputs)>;

// ── IOStateMonitor ────────────────────────────────────────────────────────────
// Active Object that periodically polls physical IO via an IOReader callback.
// Publishes IOStateEvt on IO_STATE_CHANGED_SIG whenever any input or output
// state differs from the previous scan.
class IOStateMonitor : public QP::QActive {
public:
    // reader      : callback that fills the current IO snapshot
    // poll_ticks  : polling period in QF tick counts
    explicit IOStateMonitor(IOReader reader, std::uint32_t poll_ticks) noexcept;

private:
    QP::QTimeEvt  m_pollTimer;     // periodic tick → IO_MONITOR_POLL_SIG
    IOReader      m_reader;        // injected IO read function
    std::uint32_t m_pollTicks;     // polling interval

    IOStateEvt    m_evt;           // reused static event (one publisher, cooperative kernel)

    std::unordered_map<int, bool> m_prevInputs;
    std::unordered_map<int, bool> m_prevOutputs;

    Q_STATE_DECL(initial);
    Q_STATE_DECL(monitoring);
};
