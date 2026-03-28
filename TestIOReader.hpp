#pragma once
#include "IOStateMonitor/IOStateMonitor.h"
#include <vector>
#include <algorithm>
#include <cstdio>

// ── TestStep ──────────────────────────────────────────────────────────────────

struct TestStep {
    std::unordered_map<int, bool> inputs;
    std::unordered_map<int, bool> outputs;
    const char*      description;
    std::vector<int> expected_edges;   // IDs esperados; vacío = ningún flanco
};

// ── Resultado compartido ──────────────────────────────────────────────────────
// Control escribe aquí vía setOnEdgeDetected. El test reader lee en el
// siguiente poll (QV garantiza que Control procesó el evento antes).

static std::vector<int> g_detectedEdges;
static bool             g_edgeReceived = false;

// ── Verificación ──────────────────────────────────────────────────────────────

static void verifyStep(int stepIdx, const TestStep& s) {
    bool ok;
    if (s.expected_edges.empty()) {
        ok = !g_edgeReceived;
    } else {
        std::vector<int> expected = s.expected_edges;
        std::vector<int> actual   = g_detectedEdges;
        std::sort(expected.begin(), expected.end());
        std::sort(actual.begin(),   actual.end());
        ok = g_edgeReceived && (actual == expected);
    }

    std::printf("[Test paso %d] %s  =>  %s\n",
                stepIdx,
                s.description,
                ok ? "OK" : "FALLO");

    if (!ok) {
        if (s.expected_edges.empty()) {
            std::printf("           esperado: sin flanco  /  recibido: flanco en [");
        } else {
            std::printf("           esperado: [");
            for (int id : s.expected_edges) std::printf("%d ", id);
            std::printf("]  /  recibido: [");
        }
        for (int id : g_detectedEdges) std::printf("%d ", id);
        std::printf("]\n");
    }

    // Reset para el siguiente paso
    g_detectedEdges.clear();
    g_edgeReceived = false;
}

// ── makeTestReader ────────────────────────────────────────────────────────────
// Devuelve un IOReader que avanza por la secuencia de pasos en cada poll.
// Cuando termina llama a QP::QF::stop().
//
// Casos cubiertos (edgeDetector configurado: entrada 1, flanco subida):
//
//  Paso 0 — estado inicial: entrada 1 OFF
//           Primera muestra: DigitalEdgeDetector inicializa prev=current → no flanco
//
//  Paso 1 — sin cambio: entrada 1 sigue OFF
//           IOStateMonitor no publica → no flanco
//
//  Paso 2 — flanco de SUBIDA: entrada 1 OFF→ON
//           → se espera flanco en entrada 1
//
//  Paso 3 — sin cambio: entrada 1 sigue ON
//           → no flanco
//
//  Paso 4 — flanco de BAJADA: entrada 1 ON→OFF
//           logic_positive=true → bajada ignorada → no flanco
//
//  Paso 5 — segunda SUBIDA: entrada 1 OFF→ON
//           → se espera flanco en entrada 1
//
//  Paso 6 — entrada 2 sube (no configurada) + entrada 1 baja
//           → no flanco

inline IOReader makeTestReader() {
    static const std::vector<TestStep> steps = {
        { {{1,false},{2,false}}, {{10,false}},
          "inicial entrada 1=OFF",                          {} },

        { {{1,false},{2,false}}, {{10,false}},
          "sin cambio",                                     {} },

        { {{1,true}, {2,false}}, {{10,false}},
          "SUBIDA entrada 1",                               {1} },

        { {{1,true}, {2,false}}, {{10,false}},
          "sin cambio",                                     {} },

        { {{1,false},{2,false}}, {{10,false}},
          "BAJADA entrada 1 (ignorada, logic_positive=true)",{} },

        { {{1,true}, {2,false}}, {{10,false}},
          "segunda SUBIDA entrada 1",                       {1} },

        { {{1,false},{2,true}},  {{10,false}},
          "entrada 2 sube (no configurada) + entrada 1 baja",{} },
    };

    static int step = 0;

    return [](std::unordered_map<int, bool>& inputs,
              std::unordered_map<int, bool>& outputs)
    {
        // Verificar resultado del paso anterior (excepto en el primero)
        if (step > 0) {
            verifyStep(step - 1, steps[step - 1]);
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
