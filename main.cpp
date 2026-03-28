#include "qpcpp/include/qpcpp.hpp"
#include "signals.h"
#include "IOStateMonitor/IOStateMonitor.h"
#include "DigitalEdgeDetector/DigitalEdgeDetector.h"
#include "Control/Control.h"
#include "TestIOReader.hpp"
#include <cstdio>
#include <cstdlib>

// ── QP assertion handler (requerido por el framework) ─────────────────────────
extern "C" Q_NORETURN Q_onError(char const * const module, int_t const id) {
    std::fprintf(stderr, "Q_onError: %s:%d\n", module, id);
    std::exit(1);
}

// ── QF pub/sub table ──────────────────────────────────────────────────────────
static QP::QSubscrList subscrSto[MAX_SIG];

// ── Event queues ──────────────────────────────────────────────────────────────
static QP::QEvtPtr ioMonitorQSto[10];
static QP::QEvtPtr edgeDetectorQSto[10];
static QP::QEvtPtr controlQSto[10];

// ── Active object instances ───────────────────────────────────────────────────
static IOStateMonitor ioMonitor{ makeTestReader(), 10U };

static DigitalEdgeDetector edgeDetector;
static Control             control;

// ── Callbacks requeridos por el port win32-qv ─────────────────────────────────
namespace QP {
namespace QF {

void onStartup() {
    setTickRate(10U, 50); // 10 ticks/seg, prioridad media del thread ticker
}

void onCleanup() {}

void onClockTick() {
    // Avanza todos los QTimeEvt en la tasa 0 (la única que usamos)
    QP::QTimeEvt::TICK_X(0U, nullptr);
}

} // namespace QF
} // namespace QP

// ── main ──────────────────────────────────────────────────────────────────────
int main() {
    std::printf("=== Digital IO Edge Detector ===\n");

    QP::QF::init();

    // Inicializa la tabla de suscripciones (una entrada por señal)
    QP::QActive::psInit(subscrSto, Q_DIM(subscrSto));

    // Configura el detector: entrada 1, flanco de subida, siempre activa
    edgeDetector.configure({
        InputConfig{1, /*logic_positive=*/true, /*always=*/true, {}}
    });

    // Conecta el callback de test: Control notifica los IDs detectados
    control.setOnEdgeDetected([](const std::vector<int>& ids) {
        g_detectedEdges = ids;
        g_edgeReceived  = true;
    });

    // Arranca los AOs con sus colas de eventos y prioridades
    // Prioridad 3 → IOStateMonitor  (publica IO_STATE_CHANGED_SIG)
    // Prioridad 2 → DigitalEdgeDetector (publica EDGE_DETECTED_SIG)
    // Prioridad 1 → Control (consume EDGE_DETECTED_SIG)
    ioMonitor.start(   3U, ioMonitorQSto,    Q_DIM(ioMonitorQSto),    nullptr, 0U);
    edgeDetector.start(2U, edgeDetectorQSto, Q_DIM(edgeDetectorQSto), nullptr, 0U);
    control.start(     1U, controlQSto,      Q_DIM(controlQSto),      nullptr, 0U);

    return QP::QF::run(); // cede el control al kernel QV
}
