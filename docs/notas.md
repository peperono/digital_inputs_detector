# Notas del proyecto — digital_inputs_detector

## Arquitectura

El sistema sigue el patrón **productor/suscriptor** con objetos activos QP/C++ (kernel QV cooperativo, port win32).

```
IOStateMonitor  →  DigitalEdgeDetector  →  Control
  (productor)         (filtro/productor)    (consumidor)
```

### Objetos activos

| AO | Prioridad | Publica | Suscribe |
|----|-----------|---------|----------|
| `IOStateMonitor` | 3 | `IO_STATE_CHANGED_SIG` | — |
| `DigitalEdgeDetector` | 2 | `EDGE_DETECTED_SIG` | `IO_STATE_CHANGED_SIG` |
| `Control` | 1 | — | `EDGE_DETECTED_SIG` |

### Señales (`signals.h`)

| Señal | Tipo de evento | Descripción |
|-------|---------------|-------------|
| `IO_STATE_CHANGED_SIG` | `IOStateEvt` | Estado IO cambió |
| `EDGE_DETECTED_SIG` | `EdgeDetectedEvt` | Flanco detectado en una o más entradas |
| `IO_MONITOR_POLL_SIG` | `QEvt` | Timer interno del IOStateMonitor |

### IOReader

Callback inyectado en `IOStateMonitor`:
```cpp
using IOReader = std::function<void(
    std::unordered_map<int, bool>& inputs,
    std::unordered_map<int, bool>& outputs)>;
```
En producción lee registros de hardware. En test se puede inyectar una lambda que devuelva datos simulados.

---

## Decisiones de diseño

- Los eventos `IOStateEvt` y `EdgeDetectedEvt` usan **semántica estática** (`poolNum_=0`)
  porque contienen `std::vector` / `std::unordered_map` no compatibles con los memory pools de QP.
  Es seguro en QV (cooperativo, un solo hilo).

- `IOStateMonitor` solo publica cuando el estado **cambia** respecto al ciclo anterior.

- `DigitalEdgeDetector` inicializa `prev = current` en la primera muestra de cada entrada
  para evitar flancos espurios al arrancar.

---

## Pendientes / ideas

-

---

## Estudio de C++

### Punteros y `const`

Regla para leer declaraciones: **empezar por el `*` y leer hacia los lados**.
Lo que está a la izquierda del `*` describe el **dato**; lo que está a la derecha, el **puntero**.

```cpp
int       *       p;   // puntero libre,     dato libre
int const *       p;   // puntero libre,     dato constante  ← más común en QP
int       * const p;   // puntero constante, dato libre
int const * const p;   // ambos constantes
```

En QP se usa `QEvt const*` porque el AO receptor solo lee el evento, nunca lo modifica.

### `using` — alias de tipo

Define un nombre nuevo para un tipo existente. No declara variables, solo describe la forma del tipo.

```cpp
using QEvtPtr = QEvt const*;   // QEvtPtr es ahora un nombre para "puntero a QEvt const"

QEvtPtr p;          // variable de ese tipo
QEvtPtr cola[10];   // array de 10 punteros de ese tipo
```

### `std::unordered_map<K, V>`

Contenedor asociativo de la STL: almacena pares **clave → valor** sin un orden concreto.
Internamente usa una **tabla hash**, lo que da acceso en tiempo constante O(1) de media.

```cpp
std::unordered_map<int, bool> s_outputs = {{10, true}};
//                 ───  ────    clave=10, valor=true
//                 K    V
```

| Operación | Sintaxis | Coste medio |
|-----------|----------|-------------|
| Insertar / actualizar | `m[clave] = valor` | O(1) |
| Leer | `m.at(clave)` o `m[clave]` | O(1) |
| Buscar sin insertar | `m.find(clave)` | O(1) |
| Comprobar existencia | `m.count(clave)` | O(1) |
| Iterar | `for (auto& [k, v] : m)` | O(n) |

En este proyecto se usa para mapear **ID de canal → estado booleano** (ON/OFF).
No es apto para QP memory pools porque tiene destructor no trivial.

### `Q_onError` — manejador de errores irrecuperables

```cpp
extern "C" Q_NORETURN Q_onError(char const * const module, int_t const id);
```

QP llama a esta función cuando falla alguna de sus macros de aserción internas
(`Q_ASSERT`, `Q_REQUIRE`, `Q_ENSURE`, `Q_INVARIANT`).

| Elemento | Significado |
|----------|-------------|
| `extern "C"` | Enlazado C para que QP (escrito en C) la encuentre por nombre exacto |
| `Q_NORETURN` | Expande a `[[noreturn]]`; la función nunca retorna |
| `module` | Nombre del fichero fuente donde se detectó el error (`__FILE__`) |
| `id` | Número de línea o ID del assert que falló |

En producción/embebido se sustituye `std::exit` por un reset del microcontrolador.

**Tipos de assert en QP:**

| Macro | Responsable si falla |
|-------|---------------------|
| `Q_REQUIRE` | El llamador (precondición de entrada) |
| `Q_ENSURE` | La propia función (postcondición) |
| `Q_INVARIANT` | El objeto (estado interno consistente) |
| `Q_ASSERT` | Genérico |
