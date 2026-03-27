// ── System integration test ───────────────────────────────────────────────────
// Verifica el pipeline completo:
//   JsonIOSource → IOStateMonitor → DigitalEdgeDetector → Control
//
// Cada test inyecta secuencias de JSON a través del IOReader y comprueba
// los flancos que notifica el módulo Control.

#include "qpcpp/include/qpcpp.hpp"
#include "signals.h"
#include "IOStateMonitor/IOStateMonitor.h"
#include "DigitalEdgeDetector/DigitalEdgeDetector.h"
#include "Control/Control.h"
#include "IOReaderJson.hpp"

#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

// ── QP mandatory callbacks (win32-qv port) ────────────────────────────────────

extern "C" Q_NORETURN Q_onError(char const* const module, int_t const id) {
    std::fprintf(stderr, "Q_onError %s:%d\n", module, id);
    std::exit(1);
}

namespace QP { namespace QF {
    void onStartup()  { setTickRate(100U, 30); }
    void onCleanup()  {}
    void onClockTick() {
        // No avanzamos los timers: los polls se disparan manualmente.
    }
}}

// ── Global active objects ─────────────────────────────────────────────────────

static JsonIOSource        g_ioSrc;

// poll_ticks=100000 → el timer interno nunca dispara durante el test;
// los polls se inyectan manualmente con inject().
static IOStateMonitor      g_ioMonitor{g_ioSrc.reader(), 100000U};
static DigitalEdgeDetector g_edgeDetector;
static Control             g_control;

// ── QP infrastructure ─────────────────────────────────────────────────────────

static QP::QSubscrList subscrSto[MAX_SIG];
static QP::QEvtPtr     ioMonQSto[16];
static QP::QEvtPtr     edgeQSto[16];
static QP::QEvtPtr     ctrlQSto[16];

// ── Shared result (QP thread → test thread) ───────────────────────────────────

struct TestResult {
    std::mutex              mtx;
    std::condition_variable cv;
    std::vector<int>        edges;
    bool                    received{false};
};
static TestResult g_result;

// ── Test helpers ──────────────────────────────────────────────────────────────

// Actualiza el snapshot JSON y dispara un poll manual al IOStateMonitor.
static void inject(const std::string& json) {
    g_ioSrc.update(json);
    // Evento estático (poolNum_=0): QP no lo libera, seguro para reusar.
    static QP::QEvt const s_poll{IO_MONITOR_POLL_SIG};
    g_ioMonitor.post(&s_poll, 0U);
}

// Resetea el estado compartido antes de cada test.
static void reset_result() {
    std::lock_guard<std::mutex> lk(g_result.mtx);
    g_result.received = false;
    g_result.edges.clear();
}

// Bloquea hasta que Control notifique un flanco (timeout en ms).
// Devuelve true y rellena 'ids' si llegó dentro del plazo.
static bool await_edge(std::vector<int>& ids, int timeout_ms = 500) {
    std::unique_lock<std::mutex> lk(g_result.mtx);
    bool ok = g_result.cv.wait_for(
        lk, std::chrono::milliseconds(timeout_ms),
        [&]{ return g_result.received; });
    if (ok) {
        ids = g_result.edges;
        g_result.received = false;
    }
    return ok;
}

// Espera 'timeout_ms' y devuelve true si NO llegó ningún flanco.
static bool no_edge_within(int timeout_ms = 120) {
    std::this_thread::sleep_for(std::chrono::milliseconds(timeout_ms));
    std::lock_guard<std::mutex> lk(g_result.mtx);
    bool clean = !g_result.received;
    g_result.received = false;
    return clean;
}

// ── Tests ─────────────────────────────────────────────────────────────────────

// Flanco de subida (logic_positive=true) dispara detección.
static void test_rising_edge_triggers() {
    g_edgeDetector.configure({InputConfig{1, true, true, {}}});
    reset_result();

    // Primera muestra: prev se inicializa a current → sin flanco.
    inject(R"({"inputs":{"1":false},"outputs":{}})");
    assert(no_edge_within() && "no debe haber flanco en primera muestra");

    // false → true: flanco de subida → detección.
    inject(R"({"inputs":{"1":true},"outputs":{}})");
    std::vector<int> ids;
    assert(await_edge(ids) && ids == std::vector<int>{1});

    // Se mantiene en alto: sin nuevo flanco.
    inject(R"({"inputs":{"1":true},"outputs":{}})");
    assert(no_edge_within() && "sin flanco cuando estado estable");

    std::printf("PASS  test_rising_edge_triggers\n");
}

// Flanco de bajada (logic_positive=false) dispara detección.
static void test_falling_edge_triggers() {
    g_edgeDetector.configure({InputConfig{1, false, true, {}}});
    reset_result();

    inject(R"({"inputs":{"1":true},"outputs":{}})");   // prev = true
    assert(no_edge_within());

    inject(R"({"inputs":{"1":false},"outputs":{}})");  // true → false: flanco de bajada
    std::vector<int> ids;
    assert(await_edge(ids) && ids == std::vector<int>{1});

    std::printf("PASS  test_falling_edge_triggers\n");
}

// Con logic_positive=false, un flanco de subida NO dispara.
static void test_rising_no_pulse_when_logic_negative() {
    g_edgeDetector.configure({InputConfig{1, false, true, {}}});
    reset_result();

    inject(R"({"inputs":{"1":false},"outputs":{}})");
    assert(no_edge_within());

    inject(R"({"inputs":{"1":true},"outputs":{}})");   // subida con lógica negativa → sin pulso
    assert(no_edge_within());

    std::printf("PASS  test_rising_no_pulse_when_logic_negative\n");
}

// detection_always=false: sin pulso si todas las salidas vinculadas están inactivas.
static void test_no_pulse_when_outputs_inactive() {
    g_edgeDetector.configure({InputConfig{1, true, false, {10, 11}}});
    reset_result();

    inject(R"({"inputs":{"1":false},"outputs":{"10":false,"11":false}})");
    assert(no_edge_within());

    inject(R"({"inputs":{"1":true},"outputs":{"10":false,"11":false}})");
    assert(no_edge_within() && "salidas inactivas deben suprimir la deteccion");

    std::printf("PASS  test_no_pulse_when_outputs_inactive\n");
}

// detection_always=false: pulso si al menos una salida vinculada está activa.
static void test_pulse_when_one_output_active() {
    g_edgeDetector.configure({InputConfig{1, true, false, {10, 11}}});
    reset_result();

    inject(R"({"inputs":{"1":false},"outputs":{"10":false,"11":true}})");
    assert(no_edge_within());

    inject(R"({"inputs":{"1":true},"outputs":{"10":false,"11":true}})");
    std::vector<int> ids;
    assert(await_edge(ids) && ids == std::vector<int>{1});

    std::printf("PASS  test_pulse_when_one_output_active\n");
}

// Entrada ausente del JSON → sin crash, sin pulso.
static void test_missing_input_ignored() {
    g_edgeDetector.configure({InputConfig{1, true, true, {}}});
    reset_result();

    inject(R"({"inputs":{},"outputs":{}})");   // entrada 1 no aparece en el JSON
    assert(no_edge_within());

    std::printf("PASS  test_missing_input_ignored\n");
}

// Dos entradas distintas generan flancos en el mismo ciclo de scan.
static void test_multiple_inputs_same_cycle() {
    g_edgeDetector.configure({
        InputConfig{1, true,  true, {}},   // flanco de subida
        InputConfig{2, false, true, {}},   // flanco de bajada
    });
    reset_result();

    // Establecer estados previos: 1=LOW, 2=HIGH
    inject(R"({"inputs":{"1":false,"2":true},"outputs":{}})");
    assert(no_edge_within());

    // Ambos flancos en el mismo ciclo
    inject(R"({"inputs":{"1":true,"2":false},"outputs":{}})");
    std::vector<int> ids;
    assert(await_edge(ids) && ids.size() == 2U);

    std::printf("PASS  test_multiple_inputs_same_cycle\n");
}

// ── Test thread entry ─────────────────────────────────────────────────────────

static void run_tests() {
    // Espera a que los AOs completen su transición inicial.
    std::this_thread::sleep_for(100ms);

    test_rising_edge_triggers();
    test_falling_edge_triggers();
    test_rising_no_pulse_when_logic_negative();
    test_no_pulse_when_outputs_inactive();
    test_pulse_when_one_output_active();
    test_missing_input_ignored();
    test_multiple_inputs_same_cycle();

    std::printf("\nAll tests passed.\n");
    QP::QF::stop();
}

// ── main ──────────────────────────────────────────────────────────────────────

int main() {
    std::printf("=== System Integration Test ===\n\n");

    // Registra el callback de Control ANTES de arrancar QF.
    g_control.setOnEdgeDetected([](const std::vector<int>& ids) {
        std::lock_guard<std::mutex> lk(g_result.mtx);
        g_result.edges    = ids;
        g_result.received = true;
        g_result.cv.notify_one();
    });

    QP::QF::init();
    QP::QActive::psInit(subscrSto, Q_DIM(subscrSto));

    // Configura el detector con entrada 1, flanco de subida, siempre activa.
    // (los tests lo reconfiguran antes de cada caso)
    g_edgeDetector.configure({InputConfig{1, true, true, {}}});

    g_ioMonitor.start(   3U, ioMonQSto, Q_DIM(ioMonQSto), nullptr, 0U);
    g_edgeDetector.start(2U, edgeQSto,  Q_DIM(edgeQSto),  nullptr, 0U);
    g_control.start(     1U, ctrlQSto,  Q_DIM(ctrlQSto),  nullptr, 0U);

    std::thread testThread(run_tests);
    int ret = QP::QF::run();     // bloquea hasta QF::stop()
    testThread.join();

    return ret;
}
