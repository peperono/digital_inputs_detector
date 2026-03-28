#!/usr/bin/env bash
# Ejecutar desde la terminal MSYS2 MINGW64:
#   cd /c/Users/USER/digital_inputs_detector
#   bash build.sh

ROOT="$(cd "$(dirname "$0")" && pwd)"
QPCPP="$ROOT/qpcpp"
OUT="$ROOT/build"

mkdir -p "$OUT"

echo "Compilando app..."
g++ -std=c++17 -Wall -O1 -static \
    -I"$ROOT" \
    -I"$QPCPP/include" \
    -I"$QPCPP/src" \
    -I"$QPCPP/ports/win32-qv" \
    "$ROOT/main.cpp" \
    "$ROOT/IOStateMonitor/IOStateMonitor.cpp" \
    "$ROOT/DigitalEdgeDetector/DigitalEdgeDetector.cpp" \
    "$ROOT/Control/Control.cpp" \
    "$QPCPP/src/qf/qep_hsm.cpp" \
    "$QPCPP/src/qf/qep_msm.cpp" \
    "$QPCPP/src/qf/qf_act.cpp" \
    "$QPCPP/src/qf/qf_actq.cpp" \
    "$QPCPP/src/qf/qf_defer.cpp" \
    "$QPCPP/src/qf/qf_dyn.cpp" \
    "$QPCPP/src/qf/qf_mem.cpp" \
    "$QPCPP/src/qf/qf_ps.cpp" \
    "$QPCPP/src/qf/qf_qact.cpp" \
    "$QPCPP/src/qf/qf_qeq.cpp" \
    "$QPCPP/src/qf/qf_qmact.cpp" \
    "$QPCPP/src/qf/qf_time.cpp" \
    "$QPCPP/ports/win32-qv/qf_port.cpp" \
    -o "$OUT/app.exe" \
    -lwinmm

if [ $? -eq 0 ]; then
    echo "OK — build/app.exe"
else
    echo "ERROR — revisa los mensajes de arriba"
    exit 1
fi
