#include "qpcpp/include/qpcpp.hpp"
#include "signals.h"
#include "DigitalEdgeDetector/DigitalEdgeDetector.h"
#include "Monitor/Monitor.h"
#include "HttpServer/HttpServer.h"
#include "SharedState.h"
#include "TestIOReader.hpp"
#include <cstdio>
#include <cstdlib>

SharedState g_state;

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

// ── Active object instances ───────────────────────────────────────────────────
static DigitalEdgeDetector edgeDetector{ makeTestReader(), 10U };
static Monitor             monitor;
static TestObserver        testObserver;

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

    QP::QF::init();

    QP::QActive::psInit(subscrSto, Q_DIM(subscrSto));

    edgeDetector.configure({
        InputConfig{1, /*logic_positive=*/true, /*always=*/true,  {}   },
        InputConfig{2, /*logic_positive=*/true, /*always=*/false, {10} }
    });

    // Prioridad 3 → DigitalEdgeDetector (poll IO + publica eventos)
    // Prioridad 2 → Monitor             (consume IO_STATE_CHANGED_SIG y EDGE_DETECTED_SIG)
    // Prioridad 1 → TestObserver        (observa eventos para verificación)
    edgeDetector.start(3U, edgeDetectorQSto,  Q_DIM(edgeDetectorQSto),  nullptr, 0U);
    monitor.start(     2U, controlQSto,       Q_DIM(controlQSto),       nullptr, 0U);
    testObserver.start(1U, testObserverQSto,  Q_DIM(testObserverQSto),  nullptr, 0U);

    HttpServer::start(8080);

    int ret = QP::QF::run();

    HttpServer::stop();
    return ret;
}
