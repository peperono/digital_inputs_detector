#include "qpcpp/include/qpcpp.hpp"
#include "signals.h"
#include "DigitalEdgeDetector/DigitalEdgeDetector.h"
#include "Monitor/Monitor.h"
#include "HttpServer/HttpServer.h"
#include "HttpServer/WsPublisher.h"
#include "SharedState.h"
#include "TestIOReader.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>

SharedState se;

// ── QP assertion handler (requerido por el framework) ─────────────────────────
extern "C" Q_NORETURN Q_onError(char const * const module, int_t const id) {
    std::fprintf(stderr, "Q_onError: %s:%d\n", module, id);
    std::exit(1);
}

// ── QF pub/sub table ──────────────────────────────────────────────────────────
static QP::QSubscrList subscrSto[MAX_SIG];

// ── Event queues ──────────────────────────────────────────────────────────────
static QP::QEvtPtr edgeDetectorQSto[10];
static QP::QEvtPtr controlQSto[10];
static QP::QEvtPtr testObserverQSto[10];
static QP::QEvtPtr wsPublisherQSto[10];

// ── Callbacks requeridos por el port win32-qv ─────────────────────────────────
namespace QP {
namespace QF {

void onStartup() {
    setTickRate(10U, 50); // 10 ticks/seg, prioridad media del thread ticker
}

void onCleanup() {}

void onClockTick() {
    QP::QTimeEvt::TICK_X(0U, nullptr);
}

} // namespace QF
} // namespace QP

// ── main ──────────────────────────────────────────────────────────────────────
int main() {
    std::printf("=== Digital IO Edge Detector ===\n");
    std::printf("Selecciona reader:\n");
    std::printf("  1) Test (secuencia automatica)\n");
    std::printf("  2) Control remoto (navegador web)\n");
    std::printf("> ");
    int choice = 1;
    std::scanf("%d", &choice);

    // Configuracion de entradas
    const std::vector<InputConfig> configs = {
        InputConfig{1, /*logic_positive=*/true, /*always=*/true,  {}   },
        InputConfig{2, /*logic_positive=*/true, /*always=*/false, {10} }
    };

    IOReader reader = (choice == 2) ? IOReader{} : makeTestReader();

    // ── Active object instances ───────────────────────────────────────────────
    static DigitalEdgeDetector edgeDetector{ std::move(reader), 10U };
    static Monitor             monitor;
    static WsPublisher         wsPublisher;
    static TestObserver        testObserver;

    edgeDetector.configure(configs);
    edgeDetector.setRemoteMode(choice == 2);

    // Inicializar SharedState
    {
        std::lock_guard<std::mutex> lk(se.mtx);
        se.remote_mode = (choice == 2);
        se.configs     = configs;
        // Pre-populate inputs/outputs so the browser sees the IO structure
        // on the very first WebSocket message (before the first poll event).
        for (const auto& cfg : configs) {
            se.inputs[cfg.id] = false;
            for (int out_id : cfg.linked_outputs)
                se.outputs[out_id] = false;
        }
    }

    QP::QF::init();
    QP::QActive::psInit(subscrSto, Q_DIM(subscrSto));

    // Pool para RemoteInputEvt y ReconfigureEvt — orden creciente de tamaño obligatorio en QP
    static std::uint8_t remoteInputPool[8 * sizeof(RemoteInputEvt)];
    QP::QF::poolInit(remoteInputPool, sizeof(remoteInputPool), sizeof(RemoteInputEvt));

    static std::uint8_t reconfigPool[4 * sizeof(ReconfigureEvt)];
    QP::QF::poolInit(reconfigPool, sizeof(reconfigPool), sizeof(ReconfigureEvt));

    // Prioridad 4 → DigitalEdgeDetector (poll IO + publica eventos)
    // Prioridad 3 → Monitor             (imprime por consola)
    // Prioridad 2 → WsPublisher         (actualiza SharedState → push WebSocket)
    // Prioridad 1 → TestObserver        (observa eventos para verificación, solo en modo test)
    edgeDetector.start(4U, edgeDetectorQSto, Q_DIM(edgeDetectorQSto), nullptr, 0U);
    monitor.start(     3U, controlQSto,      Q_DIM(controlQSto),      nullptr, 0U);
    wsPublisher.start( 2U, wsPublisherQSto,  Q_DIM(wsPublisherQSto),  nullptr, 0U);
    if (choice != 2) {
        testObserver.start(1U, testObserverQSto, Q_DIM(testObserverQSto), nullptr, 0U);
    }

    HttpServer::start(8080, &edgeDetector);

    int ret = QP::QF::run();

    HttpServer::stop();
    return ret;
}
