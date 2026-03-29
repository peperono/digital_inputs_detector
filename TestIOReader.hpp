#pragma once
#include "DigitalEdgeDetector/DigitalEdgeDetector.h"
#include "qpcpp/include/qpcpp.hpp"
#include "signals.h"
#include <vector>
#include <algorithm>
#include <cstdio>

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
//  Paso 2 — SUBIDA entrada 1 → IO_STATE_CHANGED_SIG + EDGE_DETECTED_SIG(1)
//  Paso 3 — sin cambio → sin eventos
//  Paso 4 — BAJADA entrada 1 → IO_STATE_CHANGED_SIG, sin flanco
//  Paso 5 — segunda SUBIDA entrada 1 → IO_STATE_CHANGED_SIG + EDGE_DETECTED_SIG(1)
//  Paso 6 — entrada 2 sube + entrada 1 baja → IO_STATE_CHANGED_SIG, sin flanco
//  Paso 7 — reset entrada 2 → IO_STATE_CHANGED_SIG, sin flanco
//  Paso 8 — SUBIDA entrada 2, salida 10=OFF → IO_STATE_CHANGED_SIG, sin flanco
//  Paso 9 — salida 10 se activa → IO_STATE_CHANGED_SIG, sin flanco
//  Paso 10 — BAJADA entrada 2, salida 10=ON → IO_STATE_CHANGED_SIG, sin flanco
//  Paso 11 — SUBIDA entrada 2, salida 10=ON → IO_STATE_CHANGED_SIG + EDGE_DETECTED_SIG(2)

inline IOReader makeTestReader() {
    static const std::vector<TestStep> steps = {
        { {{1,false},{2,false}}, {{10,false}},
          "inicial entrada 1=OFF",                            {} },

        { {{1,false},{2,false}}, {{10,false}},
          "sin cambio",                                       {} },

        { {{1,true}, {2,false}}, {{10,false}},
          "SUBIDA entrada 1",                                 {1} },

        { {{1,true}, {2,false}}, {{10,false}},
          "sin cambio",                                       {} },

        { {{1,false},{2,false}}, {{10,false}},
          "BAJADA entrada 1 (ignorada, logic_positive=true)", {} },

        { {{1,true}, {2,false}}, {{10,false}},
          "segunda SUBIDA entrada 1",                        {1} },

        { {{1,false},{2,true}},  {{10,false}},
          "entrada 2 sube + entrada 1 baja",                 {} },

        // ── Casos detection_always=false (entrada 2 vinculada a salida 10) ──

        { {{1,false},{2,false}}, {{10,false}},
          "reset entrada 2",                                  {} },

        { {{1,false},{2,true}},  {{10,false}},
          "SUBIDA entrada 2 con salida 10=OFF: flanco ignorado", {} },

        { {{1,false},{2,true}},  {{10,true}},
          "salida 10 se activa (sin cambio en entradas)",     {} },

        { {{1,false},{2,false}}, {{10,true}},
          "BAJADA entrada 2 con salida 10=ON: bajada ignorada (logic_positive)", {} },

        { {{1,false},{2,true}},  {{10,true}},
          "SUBIDA entrada 2 con salida 10=ON: flanco detectado", {2} },
    };

    static int step = 0;
    // Estado previo: mapa vacío antes del primer poll (igual que DigitalEdgeDetector)
    static std::unordered_map<int, bool> prevInputs;
    static std::unordered_map<int, bool> prevOutputs;

    return [](std::unordered_map<int, bool>& inputs,
              std::unordered_map<int, bool>& outputs)
    {
        if (step > 0) {
            verifyStep(step - 1, steps[step - 1], prevInputs, prevOutputs);
            prevInputs  = steps[step - 1].inputs;
            prevOutputs = steps[step - 1].outputs;
        }

        if (step >= static_cast<int>(steps.size())) {
            std::printf("\n=== Test completado ===\n");
            QP::QF::stop();
            return;
        }

        const TestStep& s = steps[step];
        inputs  = s.inputs;
        outputs = s.outputs;
        ++step;
    };
}
