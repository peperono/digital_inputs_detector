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

# Detectar compilador: MSYS2/nativo o cross-compiler de devcontainer
if command -v x86_64-w64-mingw32-g++ &>/dev/null; then
    GPP=x86_64-w64-mingw32-g++
    GCC=x86_64-w64-mingw32-gcc
else
    GPP=g++
    GCC=gcc
fi
echo "Compilador: $GPP"
[ "$GPP" = "x86_64-w64-mingw32-g++" ] && EXTRA_LIBS="-lwinpthread" || EXTRA_LIBS=""

# Compilar mongoose.c como C
echo "Compilando mongoose..."
$GCC -c -O1 \
    -I"$ROOT/mongoose" \
    "$ROOT/mongoose/mongoose.c" \
    -o "$OUT/mongoose.o" 2>/dev/null \
|| $GPP -c -O1 -x c \
    -I"$ROOT/mongoose" \
    "$ROOT/mongoose/mongoose.c" \
    -o "$OUT/mongoose.o"

if [ $? -ne 0 ]; then
    echo "ERROR compilando mongoose"
    exit 1
fi

echo "Compilando app..."
$GPP -std=c++17 -Wall -O1 -static \
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
    -lwinmm -lws2_32 ${EXTRA_LIBS:-}

if [ $? -eq 0 ]; then
    echo "OK — build/app.exe"
else
    echo "ERROR — revisa los mensajes de arriba"
    exit 1
fi
