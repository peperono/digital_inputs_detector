#!/usr/bin/env bash
# Genera web/index_html.h a partir de web/index.html
# Uso: bash web/gen_html_header.sh

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
INPUT="$SCRIPT_DIR/index.html"
OUTPUT="$SCRIPT_DIR/index_html.h"

if [ ! -f "$INPUT" ]; then
    echo "ERROR: no se encuentra $INPUT"
    exit 1
fi

{
    echo '// Auto-generated from web/index.html — do not edit manually'
    echo '#pragma once'
    printf 'static const char* s_html = R"html('
    cat "$INPUT"
    echo ')html";'
} > "$OUTPUT"

echo "Generado $OUTPUT"
