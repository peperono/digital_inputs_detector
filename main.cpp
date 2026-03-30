#include "qpcpp/include/qpcpp.hpp"
#include "signals.h"
#include "DigitalEdgeDetector/DigitalEdgeDetector.h"
#include "Monitor/Monitor.h"
#include "HttpServer/HttpServer.h"
#include "SharedState.h"
#include "TestIOReader.hpp"
#include "RemoteReader.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>

SharedState       g_state;
RemoteReaderState g_remoteState;

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

    IOReader reader = (choice == 2) ? makeRemoteReader() : makeTestReader();

    // ── Active object instances ───────────────────────────────────────────────
    static DigitalEdgeDetector edgeDetector{ std::move(reader), 10U };
    static Monitor             monitor;
    static TestObserver        testObserver;

    edgeDetector.configure(configs);

    // Inicializar SharedState con IDs configurados
    {
        std::lock_guard<std::mutex> lk(g_state.mtx);
        for (auto const& cfg : configs) {
            g_state.configured_inputs.push_back(cfg.id);
            for (int out : cfg.linked_outputs)
                g_state.configured_outputs.push_back(out);
        }
        std::sort(g_state.configured_outputs.begin(), g_state.configured_outputs.end());
        g_state.configured_outputs.erase(
            std::unique(g_state.configured_outputs.begin(), g_state.configured_outputs.end()),
            g_state.configured_outputs.end());
        g_state.remote_mode = (choice == 2);
    }

    // En modo remoto, inicializar g_remoteState con todos los IDs a false
    if (choice == 2) {
        std::lock_guard<std::mutex> lk(g_remoteState.mtx);
        for (auto const& cfg : configs) {
            g_remoteState.inputs[cfg.id] = false;
            for (int out : cfg.linked_outputs)
                g_remoteState.outputs[out] = false;
        }
    }

    QP::QF::init();
    QP::QActive::psInit(subscrSto, Q_DIM(subscrSto));

    // Prioridad 3 → DigitalEdgeDetector (poll IO + publica eventos)
    // Prioridad 2 → Monitor             (consume IO_STATE_CHANGED_SIG y EDGE_DETECTED_SIG)
    // Prioridad 1 → TestObserver        (observa eventos para verificación, solo en modo test)
    edgeDetector.start(3U, edgeDetectorQSto, Q_DIM(edgeDetectorQSto), nullptr, 0U);
    monitor.start(     2U, controlQSto,      Q_DIM(controlQSto),      nullptr, 0U);
    if (choice != 2) {
        testObserver.start(1U, testObserverQSto, Q_DIM(testObserverQSto), nullptr, 0U);
    }

    HttpServer::start(8080);

    int ret = QP::QF::run();

    HttpServer::stop();
    return ret;
}
