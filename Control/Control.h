#pragma once
#include "../qpcpp/include/qpcpp.hpp"
#include "../signals.h"
#include <functional>
#include <vector>

// ── Control ───────────────────────────────────────────────────────────────────
// Active Object que suscribe a EDGE_DETECTED_SIG e imprime por consola
// el ID de cada entrada que generó un flanco en el ciclo de scan.
class Control : public QP::QActive {
public:
    explicit Control() noexcept;

    // Callback opcional invocado tras procesar cada EDGE_DETECTED_SIG.
    // Permite al código de test verificar los IDs detectados.
    void setOnEdgeDetected(std::function<void(const std::vector<int>&)> cb) noexcept;

private:
    std::function<void(const std::vector<int>&)> m_onEdgeDetected;

    Q_STATE_DECL(initial);
    Q_STATE_DECL(running);
};
