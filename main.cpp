#include "qpcpp/include/qpcpp.hpp"
#include "signals.h"
#include "IOStateMonitor/IOStateMonitor.h"
#include "DigitalEdgeDetector/DigitalEdgeDetector.h"
#include <cstdio>
#include <unordered_map>

// ── QF pub/sub table ──────────────────────────────────────────────────────────
static QP::QSubscrList subscrSto[MAX_SIG];

// ── Event queues ──────────────────────────────────────────────────────────────
static QP::QEvtPtr ioMonitorQSto[10];
static QP::QEvtPtr edgeDetectorQSto[10];

// ── Simulated IO state ────────────────────────────────────────────────────────
// In a real system, the IOReader callback lería registros de hardware.
// Aquí simulamos que la entrada 1 cambia de estado cada 5 ciclos de polling.
static std::unordered_map<int, bool> s_inputs  = {{1, false}, {2, true}};
static std::unordered_map<int, bool> s_outputs = {{10, true}};
static int s_pollCount = 0;

// ── Active object instances ───────────────────────────────────────────────────
static IOStateMonitor ioMonitor{
    // IOReader: callback que llena el snapshot de IO
    [](std::unordered_map<int, bool>& inputs,
       std::unordered_map<int, bool>& outputs)
    {
        ++s_pollCount;
        if (s_pollCount % 5 == 0) {       // toggle entrada 1 cada 5 polls
            s_inputs[1] = !s_inputs[1];
            std::printf("[IOReader] entrada 1 -> %s\n",
                        s_inputs[1] ? "ON" : "OFF");
        }
        inputs  = s_inputs;
        outputs = s_outputs;
    },
    5U  // poll cada 5 ticks QF (= 500 ms con tick rate de 10 Hz)
};

static DigitalEdgeDetector edgeDetector;

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

    // Arranca los AOs con sus colas de eventos y prioridades
    // Prioridad 2 → IOStateMonitor tiene mayor prioridad (publica primero)
    // Prioridad 1 → DigitalEdgeDetector procesa el evento publicado
    ioMonitor.start(   2U, ioMonitorQSto,    Q_DIM(ioMonitorQSto),    nullptr, 0U);
    edgeDetector.start(1U, edgeDetectorQSto, Q_DIM(edgeDetectorQSto), nullptr, 0U);

    return QP::QF::run(); // cede el control al kernel QV
}
