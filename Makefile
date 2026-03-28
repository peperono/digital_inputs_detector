CXX      := /c/msys64/mingw64/bin/g++.exe
CXXFLAGS := -std=c++17 -Wall -O1

ROOT   := $(CURDIR)
QPCPP  := $(ROOT)/qpcpp

# ── Include paths ─────────────────────────────────────────────────────────────
INCLUDES := \
    -I$(ROOT) \
    -I$(QPCPP)/include \
    -I$(QPCPP)/ports/win32-qv

# ── Defines requeridos por el port win32-qv ───────────────────────────────────
DEFINES := -DQP_API_VERSION=9999

# ── Fuentes QP (core + port win32-qv) ────────────────────────────────────────
QP_SRCS := \
    $(QPCPP)/src/qf/qep_hsm.cpp \
    $(QPCPP)/src/qf/qep_msm.cpp \
    $(QPCPP)/src/qf/qf_act.cpp \
    $(QPCPP)/src/qf/qf_actq.cpp \
    $(QPCPP)/src/qf/qf_defer.cpp \
    $(QPCPP)/src/qf/qf_dyn.cpp \
    $(QPCPP)/src/qf/qf_mem.cpp \
    $(QPCPP)/src/qf/qf_ps.cpp \
    $(QPCPP)/src/qf/qf_qact.cpp \
    $(QPCPP)/src/qf/qf_qeq.cpp \
    $(QPCPP)/src/qf/qf_qmact.cpp \
    $(QPCPP)/src/qf/qf_time.cpp \
    $(QPCPP)/src/qv/qv.cpp \
    $(QPCPP)/ports/win32-qv/qf_port.cpp

# ── Fuentes de la aplicación ──────────────────────────────────────────────────
APP_SRCS := \
    $(ROOT)/IOStateMonitor/IOStateMonitor.cpp \
    $(ROOT)/DigitalEdgeDetector/DigitalEdgeDetector.cpp \
    $(ROOT)/Control/Control.cpp

# ── Targets ───────────────────────────────────────────────────────────────────
.PHONY: all app clean

all: app

app: $(APP_SRCS) $(QP_SRCS) $(ROOT)/main.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFINES) $^ -o $(ROOT)/build/app.exe -lwinmm
	@echo "OK — build/app.exe"

clean:
	rm -f $(ROOT)/build/*.exe

# Crear directorio build si no existe
$(ROOT)/build/app.exe: | $(ROOT)/build

$(ROOT)/build:
	mkdir -p $(ROOT)/build
