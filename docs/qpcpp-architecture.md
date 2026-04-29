# Arquitectura QP/C++ — Guia genèrica basada en digital_inputs_detector

## 1. Fils i scheduler

Aquest projecte usa el scheduler **QV** (cooperatiu, un sol fil d'execució per als Active Objects). El fil Mongoose és extern al framework QP.

```
┌─────────────────────────────┐   ┌──────────────────────┐
│  Fil QV (cooperatiu)        │   │  Fil extern (Mongoose)│
│  DigitalEdgeDetector  p=3   │   │  HttpServer           │
│  Monitor              p=2   │   │  accés a SharedState  │
│  TestObserver         p=1   │   │  via mutex            │
└─────────────────────────────┘   └──────────────────────┘
```

**Regles:**
- Tots els Active Objects (AO) corren al mateix fil QV; no hi ha condicions de carrera entre ells.
- Els fils externs (Mongoose, threads de HW) accedeixen a dades compartides **sempre** amb mutex.
- Mai no es crida `QF_PUBLISH` ni `QActive::POST` des d'un fil extern sense usar `QF_CRIT_ENTRY/EXIT` o els mecanismes thread-safe del port.

---

## 2. Active Objects

Cada AO és una subclasse de `QP::QActive` amb una HSM (Hierarchical State Machine).

### Esquelet mínim

```cpp
// MyAO.h
class MyAO : public QP::QActive {
public:
    explicit MyAO() noexcept
        : QP::QActive{Q_STATE_CAST(&MyAO::initial)} {}
private:
    Q_STATE_DECL(initial);
    Q_STATE_DECL(operating);
};

// MyAO.cpp
Q_STATE_DEF(MyAO, initial) {
    Q_UNUSED_PAR(e);
    subscribe(MY_SIGNAL_SIG);
    return tran(&MyAO::operating);
}

Q_STATE_DEF(MyAO, operating) {
    QP::QState status;
    switch (e->sig) {
        case Q_ENTRY_SIG: status = Q_HANDLED(); break;
        case MY_SIGNAL_SIG: /* ... */ status = Q_HANDLED(); break;
        default: status = super(&MyAO::top); break;
    }
    return status;
}
```

### Prioritats

Assigna prioritats d'alt a baix per ordre de criticitat. El número és la prioritat QP (`start()`).

| Prioritat | AO | Rol |
|---|---|---|
| N (alta) | AO productor (llegeix IO, publica events) | |
| N-1 | AO consumidor principal | |
| 1 (baixa) | AO d'observació / test | |

---

## 3. Senyals i events (`signals.h`)

Tots els senyals del sistema es defineixen en un únic fitxer `signals.h`.

```cpp
enum Signals : QP::QSignal {
    // Deixa els primers per a QP intern
    IO_STATE_CHANGED_SIG = QP::Q_USER_SIG,
    EDGE_DETECTED_SIG,
    RECONFIGURE_SIG,
    MAX_SIG
};

// Events sense dades dinàmiques (STL): memòria estàtica
struct IOStateEvt : QP::QEvt {
    std::unordered_map<int, bool> inputs;
    std::unordered_map<int, bool> outputs;
};

// Events amb payload petit i fix: pool QP
struct ReconfigureEvt : QP::QEvt {
    int  n_configs;
    // ... camps de mida fixa
};
```

**Regla de memòria d'events:**

| Conté STL (`vector`, `map`, `string`) | Usa memòria **estàtica** (member de l'AO o `static`) |
|---|---|
| Payload de mida fixa | Usa **pool QP** (`QF::poolInit` + `QF_NEW`) |

Els events estàtics són segurs sota QV perquè només hi ha un fil d'AO; no es reutilitzen fins que el publicador els torna a omplir.

---

## 4. Injecció de comportament: el patró IOReader

Per desacoblar la font de dades de la lògica de detecció, s'usa un `std::function` com a estratègia:

```cpp
using IOReader = std::function<void(
    std::unordered_map<int, bool>& inputs,
    std::unordered_map<int, bool>& outputs)>;
```

Cada plataforma o mode proporciona la seva implementació concreta:

| Instància | Fitxer | Descripció |
|---|---|---|
| `makeTestReader()` | `Test/TestController.hpp` | Seqüència automàtica de test |
| `makeRemoteReader()` | `RemoteIO/IOReader_Remot.hpp` | Llegeix de `RemoteIOState` (IU web) |
| `HWReader` | (futur) | Llegeix GPIO de hardware real |

L'AO rep el reader al constructor i el crida periòdicament via timer:

```cpp
class DigitalEdgeDetector : public QP::QActive {
    IOReader      m_reader;
    QP::QTimeEvt  m_pollTimer;
    // ...
};
```

El timer s'arma a `initial`:

```cpp
Q_STATE_DEF(DigitalEdgeDetector, initial) {
    m_pollTimer.armX(m_pollTicks, m_pollTicks);
    return tran(&DigitalEdgeDetector::operating);
}
```

---

## 5. Memòria compartida entre fils

### Patró SharedState (QV → fil extern)

Per passar dades des dels AO cap a un fil extern (ex. Mongoose per WebSocket):

```cpp
// SharedState.h
struct SharedState {
    std::mutex mtx;
    std::unordered_map<int, bool> inputs;
    std::unordered_map<int, bool> outputs;
    std::atomic<bool> push_pending{false};
    // ...
};
extern SharedState se;
```

- **L'AO escriu** sota `lock_guard<mutex>` i posa `push_pending = true`.
- **El fil extern llegeix** sota `lock_guard<mutex>` quan `push_pending` és `true`.
- Definir la instància global a `main.cpp`; declarar `extern` al fitxer de capçalera.

### Patró RemoteIOState (fil extern → IOReader)

Per passar dades des d'un fil extern cap als AO (sentit invers):

```cpp
// RemoteIOState.h
struct RemoteIOState {
    std::mutex mtx;
    std::unordered_map<int, bool> inputs;
    std::unordered_map<int, bool> outputs;
};
extern RemoteIOState remoteIO;
```

- **El fil extern escriu** sota mutex (via HTTP/WebSocket).
- **L'IOReader llegeix** sota mutex cada cop que l'AO el crida.

---

## 6. Configuració d'entrades (`InputConfig`)

Cada entrada té una configuració independent:

```cpp
struct InputConfig {
    int              id;
    bool             logic_positive;   // true=flanc pujant, false=flanc baixant
    bool             detection_always; // false=detecció condicionada a sortides
    std::vector<int> linked_outputs;   // sortides que habiliten la detecció
};
```

La llista de configuracions es passa a l'AO amb `configure()` i a `SharedState` abans d'arrencar el framework.

---

## 7. Infraestructura de test (`TestController`)

El mode de test usa tres components al mateix fil QV:

```
makeTestReader() lambda ──publica passos──► DigitalEdgeDetector
                         ◄─ g_* globals ──  TestObserver
```

- **`makeTestReader()`** retorna un lambda que avança per una llista de `TestStep` i crida `verifyStep()`.
- **`TestObserver`** (AO p=1) subscriu els senyals i omple les variables `g_*` globals.
- **`verifyStep()`** compara `g_*` amb l'esperança de cada pas i escriu el resultat al log CSV.

Format del log (`test_result.log`):
```
# Test iniciat: 2026-04-29 10:30:00
1,"Descripcio del pas",OK
2,"Altre pas",ERROR
```

**Velocitat del test:** El període de poll és `poll_ticks × (1000ms / tickRate)`. Amb `setTickRate(10U)` i `poll_ticks=1U` s'obté un poll de 100 ms. El `STEP_DELAY` del reader ha de ser ≥ període de poll perquè l'AO processi cada pas.

---

## 8. Inicialització a `main.cpp`

Ordre obligatori:

```cpp
// 1. Crear instàncies globals
SharedState   se;
RemoteIOState remoteIO;

int main() {
    // 2. Preparar IOReader i configuració
    IOReader reader = makeTestReader(); // o makeRemoteReader()
    const std::vector<InputConfig> configs = { ... };

    // 3. Crear AOs (static per evitar destrucció prematura)
    static MyProducerAO producer{ std::move(reader), poll_ticks };
    static MyConsumerAO consumer;

    // 4. Configurar AOs i SharedState
    producer.configure(configs);
    { std::lock_guard lk(se.mtx); se.configs = configs; /* inicialitzar maps */ }

    // 5. Inicialitzar QP
    QP::QF::init();
    QP::QActive::psInit(subscrSto, Q_DIM(subscrSto));

    // 6. Inicialitzar pools d'events (si n'hi ha)
    static std::uint8_t pool[N * sizeof(MyPoolEvt)];
    QP::QF::poolInit(pool, sizeof(pool), sizeof(MyPoolEvt));

    // 7. Arrencar AOs per ordre de prioritat (d'alta a baixa)
    producer.start(3U, producerQSto, Q_DIM(producerQSto), nullptr, 0U);
    consumer.start(2U, consumerQSto, Q_DIM(consumerQSto), nullptr, 0U);

    // 8. Arrencar fils externs
    HttpServer::start(8080, &producer);

    // 9. Cedir el control al scheduler QP
    int ret = QP::QF::run();

    HttpServer::stop();
    return ret;
}
```

---

## 9. Callbacks del port (`onStartup`, `onClockTick`, `onCleanup`)

Requerits pel port QV de Windows/Linux:

```cpp
namespace QP { namespace QF {
    void onStartup()   { setTickRate(10U, 50); } // 10 ticks/seg
    void onCleanup()   {}
    void onClockTick() { QP::QTimeEvt::TICK_X(0U, nullptr); }
}}
```

---

## 10. Fitxers clau del projecte de referència

| Fitxer | Contingut |
|---|---|
| `signals.h` | Tots els senyals i structs d'events |
| `InputConfig.h` | Struct de configuració d'entrada |
| `DigitalEdgeDetector/SharedState.h` | Memòria compartida QV↔Mongoose |
| `RemoteIO/RemoteIOState.h` | Memòria compartida Mongoose↔IOReader |
| `Test/TestController.hpp` | TestObserver, TestStep, makeTestReader, verifyStep, g_* |
| `main.cpp` | Instàncies globals, inicialització, start dels AOs |
| `qp_config.hpp` | Tunables del framework (`QF_MAX_ACTIVE`, `QF_MAX_EPOOL`) |
| `docs/sistema.drawio` | Diagrama d'arquitectura del sistema |
| `docs/drawio-conventions.md` | Convencions visuals dels diagrames |
