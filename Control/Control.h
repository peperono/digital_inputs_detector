#pragma once
#include "../qpcpp/include/qpcpp.hpp"
#include "../signals.h"
#include <functional>
#include <vector>

// ── Control ───────────────────────────────────────────────────────────────────
// Active Object que suscribe a EDGE_DETECTED_SIG e imprime por consola
// el ID de cada entrada que generó un flanco en el ciclo de scan.
// Opcionalmente llama a un callback (útil para tests).
class Control : public QP::QActive {
public:
    explicit Control() noexcept;

    // Registra un observer llamado desde el hilo QP en cada EdgeDetectedEvt.
    // Debe configurarse antes de arrancar el AO.
    void setOnEdgeDetected(
        std::function<void(const std::vector<int>&)> cb) noexcept;

private:
    std::function<void(const std::vector<int>&)> m_onEdgeDetected;

    Q_STATE_DECL(initial);
    Q_STATE_DECL(running);
};
