#pragma once
#include "DigitalEdgeDetector/DigitalEdgeDetector.h"
#include "../DigitalEdgeDetector/SharedState.h"
#include "qpcpp/include/qpcpp.hpp"
#include "signals.h"
#include <vector>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>

static const char* TEST_LOG_FILE = "test_result.log";

// ── TestStep ──────────────────────────────────────────────────────────────────

struct TestStep {
    std::unordered_map<int, bool> inputs;
    std::unordered_map<int, bool> outputs;
    const char*                   description;
    std::vector<int>              expected_edges;  // vacío = ningún flanco
};

// ── Resultado compartido ──────────────────────────────────────────────────────

static std::vector<int>              g_detectedEdges;
static bool                          g_edgeReceived  = false;
static std::unordered_map<int, bool> g_receivedInputs;
static std::unordered_map<int, bool> g_receivedOutputs;
static bool                          g_ioReceived    = false;

// ── TestObserver ──────────────────────────────────────────────────────────────

class TestObserver : public QP::QActive {
public:
    explicit TestObserver() noexcept
        : QP::QActive{Q_STATE_CAST(&TestObserver::initial)}
    {}

private:
    Q_STATE_DECL(initial);
    Q_STATE_DECL(observing);
};

Q_STATE_DEF(TestObserver, initial) {
    Q_UNUSED_PAR(e);
    subscribe(IO_STATE_CHANGED_SIG);
    subscribe(EDGE_DETECTED_SIG);
    return tran(&TestObserver::observing);
}

Q_STATE_DEF(TestObserver, observing) {
    QP::QState status;
    switch (e->sig) {
        case Q_ENTRY_SIG: {
            status = Q_HANDLED();
            break;
        }
        case IO_STATE_CHANGED_SIG: {
            auto const* evt   = Q_EVT_CAST(IOStateEvt);
            g_receivedInputs  = evt->inputs;
            g_receivedOutputs = evt->outputs;
            g_ioReceived      = true;
            status = Q_HANDLED();
            break;
        }
        case EDGE_DETECTED_SIG: {
            auto const* evt = Q_EVT_CAST(EdgeDetectedEvt);
            g_detectedEdges = evt->input_ids;
            g_edgeReceived  = true;
            status = Q_HANDLED();
            break;
        }
        default: {
            status = super(&TestObserver::top);
            break;
        }
    }
    return status;
}

// ── Verificación ──────────────────────────────────────────────────────────────

static void verifyStep(int stepIdx, const TestStep& s,
                       const std::unordered_map<int, bool>& prevInputs,
                       const std::unordered_map<int, bool>& prevOutputs)
{
    // IO_STATE_CHANGED_SIG se espera si el estado cambió respecto al paso anterior
    bool expected_io = (s.inputs != prevInputs) || (s.outputs != prevOutputs);
    bool io_ok = (expected_io == g_ioReceived);
    if (expected_io && g_ioReceived) {
        io_ok = (g_receivedInputs == s.inputs) && (g_receivedOutputs == s.outputs);
    }

    // EDGE_DETECTED_SIG
    bool edge_ok;
    if (s.expected_edges.empty()) {
        edge_ok = !g_edgeReceived;
    } else {
        std::vector<int> expected = s.expected_edges;
        std::vector<int> actual   = g_detectedEdges;
        std::sort(expected.begin(), expected.end());
        std::sort(actual.begin(),   actual.end());
        edge_ok = g_edgeReceived && (actual == expected);
    }

    bool ok = io_ok && edge_ok;

    std::printf("[Test paso %d] %s  =>  %s\n",
                stepIdx, s.description, ok ? "OK" : "FALLO");

    if (!io_ok) {
        std::printf("  IO_STATE: esperado=%s  recibido=%s\n",
                    expected_io      ? "SI" : "NO",
                    g_ioReceived     ? "SI" : "NO");
    }
    if (!edge_ok) {
        if (s.expected_edges.empty()) {
            std::printf("  EDGE: esperado: sin flanco  /  recibido: [");
        } else {
            std::printf("  EDGE: esperado: [");
            for (int id : s.expected_edges) std::printf("%d ", id);
            std::printf("]  /  recibido: [");
        }
        for (int id : g_detectedEdges) std::printf("%d ", id);
        std::printf("]\n");
    }

    // Escriure resultat al fitxer de log
    if (FILE* f = std::fopen(TEST_LOG_FILE, "a")) {
        std::fprintf(f, "%d|%s|%s\n", stepIdx + 1, s.description, ok ? "OK" : "ERROR");
        std::fclose(f);
    }

    // Reset para el siguiente paso
    g_detectedEdges.clear();
    g_edgeReceived = false;
    g_receivedInputs.clear();
    g_receivedOutputs.clear();
    g_ioReceived = false;
}

// ── makeTestReader ────────────────────────────────────────────────────────────
// Configuración: entrada 1 (always=true), entrada 2 (always=false, linked=salida 10)
//
//  Paso 0 — estado inicial: entrada 1 OFF
//           IO_STATE_CHANGED_SIG, sin flanco
//  Paso 1 — sin cambio → sin eventos
//  Paso 2 — TANCAMENT entrada 1 → IO_STATE_CHANGED_SIG + EDGE_DETECTED_SIG(1)
//  Paso 3 — sin cambio → sin eventos
//  Paso 4 — OBERTURA entrada 1 → IO_STATE_CHANGED_SIG, sin flanco
//  Paso 5 — segon TANCAMENT entrada 1 → IO_STATE_CHANGED_SIG + EDGE_DETECTED_SIG(1)
//  Paso 6 — entrada 2 tanca + entrada 1 obre → IO_STATE_CHANGED_SIG, sin flanco
//  Paso 7 — reset entrada 2 → IO_STATE_CHANGED_SIG, sin flanco
//  Paso 8 — TANCAMENT entrada 2, sortida 10=OFF → IO_STATE_CHANGED_SIG, sin flanco
//  Paso 9 — sortida 10 s'activa → IO_STATE_CHANGED_SIG, sin flanco
//  Paso 10 — OBERTURA entrada 2, sortida 10=ON → IO_STATE_CHANGED_SIG, sin flanco
//  Paso 11 — TANCAMENT entrada 2, sortida 10=ON → IO_STATE_CHANGED_SIG + EDGE_DETECTED_SIG(2)

inline IOReader makeTestReader() {
    static const std::vector<TestStep> steps = {
        { {{1,false},{2,false}}, {{10,false}},
          "Estat inicial: E1=OBERT, E2=OBERT, S10=OBERT",                            {} },

        { {{1,false},{2,false}}, {{10,false}},
          "(Sense canvis) => (sense events)",                                       {} },

        { {{1,true}, {2,false}}, {{10,false}},
          " (E1=TANCAT) => (flanc E1)",                              {1} },

        { {{1,true}, {2,false}}, {{10,false}},
          "(Sense canvis) => (sense events)",                                       {} },

        { {{1,false},{2,false}}, {{10,false}},
          "(E1 = OBERT) => (sense events)", {} },

        { {{1,true}, {2,false}}, {{10,false}},
          "(E1 = TANCAT) => (flanc E1)",                        {1} },

        { {{1,false},{2,true}},  {{10,false}},
          "(E1 = OBERT, E2 = TANCAT) ==> (sense events) ",                 {} },

        // ── Casos detection_always=false (entrada 2 vinculada a salida 10) ──

        { {{1,false},{2,false}}, {{10,false}},
          "(E2 = OBERT) => (sense events)",                                  {} },

        { {{1,false},{2,true}},  {{10,true}},
          "(E2=TANCAT, S10=TANCAT) => (flanc E2)", {2} },

        { {{1,false},{2,false}}, {{10,true}},
          "(E2 = OBERT) => (sense events)", {} },
    };

    static int step = 0;
    // Estado previo: mapa vacío antes del primer poll (igual que DigitalEdgeDetector)
    static std::unordered_map<int, bool> prevInputs;
    static std::unordered_map<int, bool> prevOutputs;

    return [](std::unordered_map<int, bool>& inputs,
              std::unordered_map<int, bool>& outputs)
    {
        static auto stepTime = std::chrono::steady_clock::now();
        static const std::chrono::milliseconds STEP_DELAY{200};
        static const std::chrono::milliseconds INIT_DELAY{200};
        static std::unordered_map<int, bool> lastInputs;
        static std::unordered_map<int, bool> lastOutputs;
        static bool announced = false;

        auto now     = std::chrono::steady_clock::now();
        auto elapsed = now - stepTime;

        // ── Anunciar el pas abans d'esperar ──────────────────────────────────
        if (!announced) {
            if (step < static_cast<int>(steps.size()))
                std::printf("[Test] Pas %d: %s\n", step + 1, steps[step].description);
            announced = true;
            stepTime  = now;
            elapsed   = std::chrono::seconds(0);
        }

        // ── Esperar el delay ─────────────────────────────────────────────────
        auto delay = (step == 0) ? INIT_DELAY : STEP_DELAY;
        inputs  = lastInputs;
        outputs = lastOutputs;
        if (elapsed < delay) return;

        // ── Executar el pas ──────────────────────────────────────────────────
        announced = false;

        if (step > 0) {
            verifyStep(step - 1, steps[step - 1], prevInputs, prevOutputs);
            prevInputs  = steps[step - 1].inputs;
            prevOutputs = steps[step - 1].outputs;
        }

        if (step >= static_cast<int>(steps.size())) {
            std::printf("\n=== Test completat ===\n");
            QP::QF::stop();
            return;
        }

        inputs      = steps[step].inputs;
        outputs     = steps[step].outputs;
        lastInputs  = steps[step].inputs;
        lastOutputs = steps[step].outputs;
        ++step;
    };
}
