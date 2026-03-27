#pragma once
#include "../qpcpp/include/qpcpp.hpp"
#include "../signals.h"

// ── Control ───────────────────────────────────────────────────────────────────
// Active Object que suscribe a EDGE_DETECTED_SIG e imprime por consola
// el ID de cada entrada que generó un flanco en el ciclo de scan.
class Control : public QP::QActive {
public:
    explicit Control() noexcept;

private:
    Q_STATE_DECL(initial);
    Q_STATE_DECL(running);
};
