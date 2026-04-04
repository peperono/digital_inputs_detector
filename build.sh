#!/usr/bin/env bash
# Ejecutar desde la terminal MSYS2 MINGW64:
#   cd /c/Users/USER/digital_inputs_detector
#   bash build.sh

export TEMP=/tmp
export TMP=/tmp
export TMPDIR=/tmp

ROOT="$(cd "$(dirname "$0")" && pwd)"
QPCPP="$ROOT/qpcpp"
OUT="$ROOT/build"

mkdir -p "$OUT"

# Generar header HTML desde web/index.html
echo "Generando web/index_html.h..."
bash "$ROOT/web/gen_html_header.sh"
if [ $? -ne 0 ]; then
    echo "ERROR generando index_html.h"
    exit 1
fi

# Compilar mongoose.c como C
echo "Compilando mongoose..."
gcc -c -O1 \
    -I"$ROOT/mongoose" \
    "$ROOT/mongoose/mongoose.c" \
    -o "$OUT/mongoose.o" 2>/dev/null \
|| g++ -c -O1 -x c \
    -I"$ROOT/mongoose" \
    "$ROOT/mongoose/mongoose.c" \
    -o "$OUT/mongoose.o"

if [ $? -ne 0 ]; then
    echo "ERROR compilando mongoose"
    exit 1
fi

echo "Compilando app..."
g++ -std=c++17 -Wall -O1 -static \
    -I"$ROOT" \
    -I"$QPCPP/include" \
    -I"$QPCPP/src" \
    -I"$QPCPP/ports/win32-qv" \
    "$ROOT/main.cpp" \
    "$ROOT/DigitalEdgeDetector/DigitalEdgeDetector.cpp" \
    "$ROOT/Monitor/Monitor.cpp" \
    "$ROOT/HttpServer/HttpServer.cpp" \
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
    "$OUT/mongoose.o" \
    -o "$OUT/app.exe" \
    -lwinmm -lws2_32

if [ $? -eq 0 ]; then
    echo "OK — build/app.exe"
else
    echo "ERROR — revisa los mensajes de arriba"
    exit 1
fi
