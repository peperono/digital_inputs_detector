#include "HttpServer.h"
#include "../SharedState.h"
#include "../signals.h"
#include <thread>
#include <atomic>
#include <string>
#include <cstdio>
#include <cstring>

extern "C" {
#include "../mongoose/mongoose.h"
}

// ── HTML embebido ─────────────────────────────────────────────────────────────

static const char* s_html = R"html(<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Digital IO Monitor</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body { font-family: 'Courier New', monospace; background: #0d1117; color: #c9d1d9; padding: 24px; }
    h1 { color: #58a6ff; margin-bottom: 8px; }
    h2 { color: #8b949e; font-size: 13px; text-transform: uppercase; letter-spacing: 1px; margin-bottom: 8px; }
    #status { margin-bottom: 24px; font-size: 14px; }
    .ok  { color: #3fb950; }
    .err { color: #f85149; }
    .section { display: inline-block; vertical-align: top; margin-right: 32px; margin-bottom: 24px; }
    table { border-collapse: collapse; min-width: 140px; }
    th, td { border: 1px solid #30363d; padding: 6px 16px; text-align: center; font-size: 14px; }
    th { background: #161b22; color: #8b949e; }
    .on  { color: #3fb950; font-weight: bold; }
    .off { color: #f85149; }
    #edges-log { background: #161b22; border: 1px solid #30363d; padding: 8px 12px;
                 height: 160px; overflow-y: auto; font-size: 13px; list-style: none; }
    #edges-log li { padding: 3px 0; border-bottom: 1px solid #21262d; color: #d2a679; }
    #edges-log li:last-child { border-bottom: none; }
    hr { border: none; border-top: 1px solid #30363d; margin: 24px 0; }
    #ctrl-section { display: none; }
    .ctrl-group { margin-bottom: 16px; }
    .ctrl-label { color: #8b949e; font-size: 12px; text-transform: uppercase;
                  letter-spacing: 1px; margin-bottom: 6px; }
    .toggle { padding: 8px 20px; cursor: pointer; border: 1px solid #30363d;
              background: #161b22; font-family: 'Courier New', monospace;
              font-size: 13px; margin: 4px; color: #c9d1d9; }
    .toggle.on  { background: #1a4a2a; color: #3fb950; border-color: #3fb950; }
    .toggle.off { background: #3a1a1a; color: #f85149; border-color: #f85149; }
    #cfg-editor { width: 420px; height: 180px; background: #161b22; color: #c9d1d9;
                  border: 1px solid #30363d; font-family: 'Courier New', monospace;
                  font-size: 12px; padding: 8px; resize: vertical; display: block;
                  margin-bottom: 8px; }
    .cfg-btn { padding: 6px 18px; cursor: pointer; border: 1px solid #30363d;
               background: #161b22; font-family: 'Courier New', monospace;
               font-size: 13px; margin-right: 8px; color: #c9d1d9; }
    .cfg-btn:hover { border-color: #58a6ff; color: #58a6ff; }
    #cfg-status { font-size: 12px; }
  </style>
</head>
<body>
  <h1>Digital IO Monitor</h1>
  <div id="status" class="err">&#9679; Desconectado</div>

  <div class="section">
    <h2>Entradas</h2>
    <table id="tbl-inputs"><tr><th>ID</th><th>Estado</th><th>Flancos</th></tr></table>
  </div>

  <div class="section">
    <h2>Salidas</h2>
    <table id="tbl-outputs"><tr><th>ID</th><th>Estado</th></tr></table>
  </div>

  <div class="section">
    <h2>Flancos detectados</h2>
    <ul id="edges-log"></ul>
  </div>

  <hr>
  <div style="display:flex;gap:48px;align-items:flex-start;">
    <div id="ctrl-section">
      <h2>Control remoto</h2>
      <div class="ctrl-group">
        <div class="ctrl-label">Entradas</div>
        <div id="ctrl-inputs"></div>
      </div>
      <div class="ctrl-group">
        <div class="ctrl-label">Salidas</div>
        <div id="ctrl-outputs"></div>
      </div>
    </div>
    <div>
      <h2>Configuración de entradas</h2>
      <textarea id="cfg-editor" spellcheck="false"></textarea>
      <div>
        <button id="btn-cargar"  class="cfg-btn" onclick="loadConfig()">Cargar</button>
        <button id="btn-aplicar" class="cfg-btn" onclick="applyConfig()">Aplicar</button>
        <span id="cfg-status"></span>
      </div>
    </div>
  </div>

  <script>
    let ws = null;
    let controlsReady = false;
    const controlState = { inputs: {}, outputs: {} };

    const statusEl    = document.getElementById('status');
    const inputsTbl   = document.getElementById('tbl-inputs');
    const outputsTbl  = document.getElementById('tbl-outputs');
    const edgesLog    = document.getElementById('edges-log');
    const ctrlSection = document.getElementById('ctrl-section');
    const ctrlInputs  = document.getElementById('ctrl-inputs');
    const ctrlOutputs = document.getElementById('ctrl-outputs');

    function updateInputsTable(inputs, counts) {
      while (inputsTbl.rows.length > 1) inputsTbl.deleteRow(1);
      Object.entries(inputs)
        .sort(([a],[b]) => Number(a)-Number(b))
        .forEach(([id, state]) => {
          const row = inputsTbl.insertRow();
          row.insertCell().textContent = id;
          const cell = row.insertCell();
          cell.textContent = state ? 'ON' : 'OFF';
          cell.className   = state ? 'on' : 'off';
          row.insertCell().textContent = counts[id] ?? 0;
        });
    }

    function updateOutputsTable(outputs) {
      while (outputsTbl.rows.length > 1) outputsTbl.deleteRow(1);
      Object.entries(outputs)
        .sort(([a],[b]) => Number(a)-Number(b))
        .forEach(([id, state]) => {
          const row = outputsTbl.insertRow();
          row.insertCell().textContent = id;
          const cell = row.insertCell();
          cell.textContent = state ? 'ON' : 'OFF';
          cell.className   = state ? 'on' : 'off';
        });
    }

    function addEdgeEntry(edges) {
      const ts = new Date().toLocaleTimeString();
      const li = document.createElement('li');
      li.textContent = ts + ' \u2014 entrada(s): ' + edges.join(', ');
      edgesLog.insertBefore(li, edgesLog.firstChild);
    }

    function sendControl() {
      if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send(JSON.stringify(controlState));
      }
    }

    function makeToggle(container, label, id, stateMap) {
      const btn = document.createElement('button');
      btn.className = 'toggle off';
      btn.textContent = label + ' ' + id + ': OFF';
      stateMap[id] = false;
      btn.onclick = () => {
        stateMap[id] = !stateMap[id];
        const s = stateMap[id];
        btn.textContent = label + ' ' + id + ': ' + (s ? 'ON' : 'OFF');
        btn.className = 'toggle ' + (s ? 'on' : 'off');
        sendControl();
      };
      container.appendChild(btn);
    }

    function initControls(inputs, outputs) {
      Object.keys(inputs).sort((a,b)  => Number(a)-Number(b))
        .forEach(id => makeToggle(ctrlInputs,  'Entrada', Number(id), controlState.inputs));
      Object.keys(outputs).sort((a,b) => Number(a)-Number(b))
        .forEach(id => makeToggle(ctrlOutputs, 'Salida',  Number(id), controlState.outputs));
      ctrlSection.style.display = 'block';
      sendControl();
    }

    function connect() {
      ws = new WebSocket('ws://' + location.host + '/ws');
      ws.onopen  = () => {
        statusEl.textContent = '\u25CF Conectado';
        statusEl.className   = 'ok';
        loadConfig();
      };
      ws.onclose = () => {
        ws = null;
        statusEl.textContent = '\u25CF Desconectado \u2014 reintentando...';
        statusEl.className   = 'err';
        setTimeout(connect, 2000);
      };
      ws.onerror = () => ws.close();
      ws.onmessage = ({ data }) => {
        const d = JSON.parse(data);
        if (!controlsReady && d.inputs && d.outputs) {
          initControls(d.inputs, d.outputs);
          controlsReady = true;
        }
        if (d.inputs)  updateInputsTable(d.inputs, d.edge_counts || {});
        if (d.outputs) updateOutputsTable(d.outputs);
        if (d.last_edges && d.last_edges.length > 0) addEdgeEntry(d.last_edges);
      };
    }

    const cfgEditor  = document.getElementById('cfg-editor');
    const cfgStatus  = document.getElementById('cfg-status');

    const btnCargar  = document.getElementById('btn-cargar');
    const btnAplicar = document.getElementById('btn-aplicar');

    function setBusy(btn, label) {
      if (!btn) return;
      btn.textContent = label;
      btn.style.borderColor = '#d2a679';
      btn.style.color       = '#d2a679';
      btn.disabled = true;
    }
    function setIdle(btn, label) {
      if (!btn) return;
      btn.textContent = label;
      btn.style.borderColor = '';
      btn.style.color       = '';
      btn.disabled = false;
    }
    function fetchWithTimeout(url, opts, ms) {
      const ctrl = new AbortController();
      const tid  = setTimeout(() => ctrl.abort(), ms);
      return fetch(url, { ...opts, signal: ctrl.signal })
        .finally(() => clearTimeout(tid));
    }

    function loadConfig() {
      setBusy(btnCargar, 'Cargando...');
      fetchWithTimeout('/config', {}, 5000)
        .then(r => r.json())
        .then(d => {
          cfgEditor.value = JSON.stringify(d, null, 2);
          cfgStatus.textContent = 'Cargado';
          cfgStatus.className = 'ok';
        })
        .catch(() => { cfgStatus.textContent = 'Error al cargar'; cfgStatus.className = 'err'; })
        .finally(() => setIdle(btnCargar, 'Cargar'));
    }

    function applyConfig() {
      let parsed;
      try { parsed = JSON.parse(cfgEditor.value); } catch(e) {
        cfgStatus.textContent = 'JSON inválido'; cfgStatus.className = 'err'; return;
      }
      setBusy(btnAplicar, 'Aplicando...');
      fetchWithTimeout('/config', { method: 'PUT',
                                    headers: { 'Content-Type': 'application/json' },
                                    body: JSON.stringify(parsed) }, 5000)
        .then(r => {
          cfgStatus.textContent = r.ok ? 'Aplicado' : 'Error ' + r.status;
          cfgStatus.className = r.ok ? 'ok' : 'err';
        })
        .catch(() => { cfgStatus.textContent = 'Error de red'; cfgStatus.className = 'err'; })
        .finally(() => setIdle(btnAplicar, 'Aplicar'));
    }

    connect();
  </script>
</body>
</html>
)html";

// ── JSON helpers ──────────────────────────────────────────────────────────────

static std::string bool_map_to_json(const std::unordered_map<int, bool>& m) {
    std::string s = "{";
    bool first = true;
    for (auto const& [k, v] : m) {
        if (!first) s += ",";
        s += "\"" + std::to_string(k) + "\":" + (v ? "true" : "false");
        first = false;
    }
    s += "}";
    return s;
}

static std::string int_map_to_json(const std::unordered_map<int, int>& m) {
    std::string s = "{";
    bool first = true;
    for (auto const& [k, v] : m) {
        if (!first) s += ",";
        s += "\"" + std::to_string(k) + "\":" + std::to_string(v);
        first = false;
    }
    s += "}";
    return s;
}

static std::string int_vec_to_json(const std::vector<int>& v) {
    std::string s = "[";
    for (std::size_t i = 0; i < v.size(); ++i) {
        if (i > 0) s += ",";
        s += std::to_string(v[i]);
    }
    s += "]";
    return s;
}

static std::string build_ws_msg(const std::unordered_map<int, bool>& inputs,
                                 const std::unordered_map<int, bool>& outputs,
                                 const std::vector<int>&               edges,
                                 const std::unordered_map<int, int>&   counts)
{
    return "{\"inputs\":"      + bool_map_to_json(inputs)
         + ",\"outputs\":"     + bool_map_to_json(outputs)
         + ",\"last_edges\":"  + int_vec_to_json(edges)
         + ",\"edge_counts\":" + int_map_to_json(counts) + "}";
}

// ── Parser JSON entrante {"1":true,"2":false} → unordered_map ─────────────────

static void parse_bool_object(struct mg_str s, std::unordered_map<int, bool>& result) {
    const char* p   = s.buf;
    const char* end = s.buf + s.len;
    while (p < end && *p != '{') ++p;
    if (p >= end) return;
    ++p; // skip '{'
    while (p < end) {
        while (p < end && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) ++p;
        if (p >= end || *p == '}') break;
        if (*p != '"') { ++p; continue; }
        ++p; // skip opening "
        const char* ks = p;
        while (p < end && *p != '"') ++p;
        int key = std::atoi(std::string(ks, p - ks).c_str());
        if (p < end) ++p; // skip closing "
        while (p < end && *p != ':') ++p;
        if (p < end) ++p; // skip ':'
        while (p < end && (*p == ' ' || *p == '\t')) ++p;
        if (p + 4 <= end && std::strncmp(p, "true",  4) == 0) { result[key] = true;  p += 4; }
        else if (p + 5 <= end && std::strncmp(p, "false", 5) == 0) { result[key] = false; p += 5; }
        while (p < end && *p != ',' && *p != '}') ++p;
        if (p < end && *p == ',') ++p;
    }
}

// ── push_pending → push WebSocket ────────────────────────────────────────────

static void push_if_pending(struct mg_mgr* mgr) {
    if (!g_state.push_pending.load()) return;
    g_state.push_pending.store(false);

    std::unordered_map<int, bool> inputs, outputs;
    std::unordered_map<int, int>  counts;
    std::vector<int>              edges;
    {
        std::lock_guard<std::mutex> lk(g_state.mtx);
        inputs  = g_state.inputs;
        outputs = g_state.outputs;
        edges   = g_state.last_edges;
        counts  = g_state.edge_counts;
    }

    std::string msg = build_ws_msg(inputs, outputs, edges, counts);

    for (struct mg_connection* c = mgr->conns; c != nullptr; c = c->next) {
        if (c->is_websocket) {
            mg_ws_send(c, msg.c_str(), msg.size(), WEBSOCKET_OP_TEXT);
        }
    }
}

// ── Mongoose event handler ────────────────────────────────────────────────────

static QP::QActive* s_edgeDetector = nullptr;

static int  uri_tail_id(struct mg_str uri);
static void post_reconfigure(const std::vector<InputConfig>& configs);

static void http_fn(struct mg_connection* c, int ev, void* ev_data) {
    if (ev == MG_EV_HTTP_MSG) {
        auto* hm = static_cast<struct mg_http_message*>(ev_data);

        if (mg_match(hm->uri, mg_str("/ws"), NULL)) {
            mg_ws_upgrade(c, hm, NULL);
            // Enviar estado actual al cliente recién conectado
            std::unordered_map<int, bool> inputs, outputs;
            std::unordered_map<int, int>  counts;
            {
                std::lock_guard<std::mutex> lk(g_state.mtx);
                inputs  = g_state.inputs;
                outputs = g_state.outputs;
                counts  = g_state.edge_counts;
            }
            std::string msg = build_ws_msg(inputs, outputs, {}, counts);
            mg_ws_send(c, msg.c_str(), msg.size(), WEBSOCKET_OP_TEXT);

        } else if (mg_match(hm->uri, mg_str("/config"), NULL)
                   && mg_match(hm->method, mg_str("GET"), NULL)) {
            std::string body;
            {
                std::lock_guard<std::mutex> lk(g_state.mtx);
                body = "[";
                for (std::size_t i = 0; i < g_state.configs.size(); ++i) {
                    const auto& cfg = g_state.configs[i];
                    if (i > 0) body += ",";
                    body += "{\"id\":"               + std::to_string(cfg.id);
                    body += ",\"logic_positive\":"   + std::string(cfg.logic_positive   ? "true" : "false");
                    body += ",\"detection_always\":" + std::string(cfg.detection_always ? "true" : "false");
                    body += ",\"linked_outputs\":"   + int_vec_to_json(cfg.linked_outputs) + "}";
                }
                body += "]";
            }
            mg_http_reply(c, 200, "Content-Type: application/json\r\n", "%s", body.c_str());

        } else if (mg_match(hm->uri, mg_str("/config"), NULL)
                   && mg_match(hm->method, mg_str("PUT"), NULL)) {
            // Reemplazar toda la configuración con el array recibido
            std::vector<InputConfig> allConfigs;
            for (int i = 0; i < ReconfigureEvt::MAX_CONFIGS; ++i) {
                char arrpath[16];
                std::snprintf(arrpath, sizeof(arrpath), "$[%d]", i);
                int elen = 0;
                int eoff = mg_json_get(hm->body, arrpath, &elen);
                if (eoff < 0) break;
                struct mg_str elem = { hm->body.buf + eoff, (size_t)elen };
                long id = mg_json_get_long(elem, "$.id", -1);
                if (id < 0) break;
                InputConfig cfg;
                cfg.id = (int)id;
                { bool v = false; mg_json_get_bool(elem, "$.logic_positive",   &v); cfg.logic_positive   = v; }
                { bool v = false; mg_json_get_bool(elem, "$.detection_always", &v); cfg.detection_always = v; }
                allConfigs.push_back(cfg);
            }
            if (allConfigs.empty()) { mg_http_reply(c, 400, "", "empty or invalid array\n"); return; }
            post_reconfigure(allConfigs);
            mg_http_reply(c, 200, "Content-Type: application/json\r\n", "{}");

        } else if (mg_match(hm->uri, mg_str("/config"), NULL)
                   && mg_match(hm->method, mg_str("POST"), NULL)) {
            // Parsear nueva InputConfig del body y añadirla
            InputConfig cfg;
            cfg.id = (int)mg_json_get_long(hm->body, "$.id", -1);
            { bool v = false; mg_json_get_bool(hm->body, "$.logic_positive",   &v); cfg.logic_positive   = v; }
            { bool v = false; mg_json_get_bool(hm->body, "$.detection_always", &v); cfg.detection_always = v; }
            if (cfg.id < 0) { mg_http_reply(c, 400, "", "missing id\n"); return; }

            std::vector<InputConfig> allConfigs;
            {
                std::lock_guard<std::mutex> lk(g_state.mtx);
                allConfigs = g_state.configs;
            }
            allConfigs.push_back(cfg);
            post_reconfigure(allConfigs);
            mg_http_reply(c, 201, "Content-Type: application/json\r\n", "{}");

        } else if (mg_match(hm->uri, mg_str("/config/*"), NULL)
                   && mg_match(hm->method, mg_str("PUT"), NULL)) {
            int id = uri_tail_id(hm->uri);
            InputConfig cfg;
            cfg.id = id;
            { bool v = false; mg_json_get_bool(hm->body, "$.logic_positive",   &v); cfg.logic_positive   = v; }
            { bool v = false; mg_json_get_bool(hm->body, "$.detection_always", &v); cfg.detection_always = v; }

            std::vector<InputConfig> allConfigs;
            {
                std::lock_guard<std::mutex> lk(g_state.mtx);
                allConfigs = g_state.configs;
            }
            bool found = false;
            for (auto& c2 : allConfigs) {
                if (c2.id == id) { c2 = cfg; found = true; break; }
            }
            if (!found) { mg_http_reply(c, 404, "", "not found\n"); return; }
            post_reconfigure(allConfigs);
            mg_http_reply(c, 200, "Content-Type: application/json\r\n", "{}");

        } else if (mg_match(hm->uri, mg_str("/config/*"), NULL)
                   && mg_match(hm->method, mg_str("DELETE"), NULL)) {
            int id = uri_tail_id(hm->uri);
            std::vector<InputConfig> allConfigs;
            {
                std::lock_guard<std::mutex> lk(g_state.mtx);
                allConfigs = g_state.configs;
            }
            bool found = false;
            for (auto it = allConfigs.begin(); it != allConfigs.end(); ++it) {
                if (it->id == id) { allConfigs.erase(it); found = true; break; }
            }
            if (!found) { mg_http_reply(c, 404, "", "not found\n"); return; }
            post_reconfigure(allConfigs);
            mg_http_reply(c, 200, "Content-Type: application/json\r\n", "{}");

        } else if (mg_match(hm->uri, mg_str("/state"), NULL)) {
            std::string body;
            {
                std::lock_guard<std::mutex> lk(g_state.mtx);
                body  = "{\"inputs\":"  + bool_map_to_json(g_state.inputs);
                body += ",\"outputs\":" + bool_map_to_json(g_state.outputs) + "}";
            }
            mg_http_reply(c, 200, "Content-Type: application/json\r\n", "%s", body.c_str());

        } else if (mg_match(hm->uri, mg_str("/"), NULL)) {
            // Servir HTML sin pasar por printf (evita interpretar % del CSS)
            mg_printf(c, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nCache-Control: no-cache\r\nContent-Length: %u\r\n\r\n",
                      (unsigned)std::strlen(s_html));
            mg_send(c, s_html, std::strlen(s_html));

        } else {
            mg_http_reply(c, 404, "", "Not found\n");
        }

    } else if (ev == MG_EV_WS_MSG) {
        // Mensaje entrante del browser → postear REMOTE_INPUT_SIG al AO
        auto* wm = static_cast<struct mg_ws_message*>(ev_data);
        if ((wm->flags & 0xF) != WEBSOCKET_OP_TEXT) return;

        std::unordered_map<int, bool> inputs, outputs;
        int ilen = 0, olen = 0;
        int ioff = mg_json_get(wm->data, "$.inputs",  &ilen);
        int ooff = mg_json_get(wm->data, "$.outputs", &olen);

        if (ioff > 0) parse_bool_object({wm->data.buf + ioff, (size_t)ilen}, inputs);
        if (ooff > 0) parse_bool_object({wm->data.buf + ooff, (size_t)olen}, outputs);

        if ((!inputs.empty() || !outputs.empty()) && s_edgeDetector) {
            auto* evt    = Q_NEW(RemoteInputEvt, REMOTE_INPUT_SIG);
            evt->n_inputs  = 0;
            evt->n_outputs = 0;
            for (auto const& [id, v] : inputs)
                if (evt->n_inputs  < RemoteInputEvt::MAX_IOS)
                    evt->inputs[evt->n_inputs++]   = {id, v};
            for (auto const& [id, v] : outputs)
                if (evt->n_outputs < RemoteInputEvt::MAX_IOS)
                    evt->outputs[evt->n_outputs++] = {id, v};
            s_edgeDetector->post_(evt, nullptr);
        }
    }
}

// ── Helpers para CRUD /config ─────────────────────────────────────────────────

static int uri_tail_id(struct mg_str uri) {
    const char* p = uri.buf + uri.len;
    while (p > uri.buf && *(p-1) != '/') --p;
    return std::atoi(p);
}

static void post_reconfigure(const std::vector<InputConfig>& configs) {
    if (!s_edgeDetector) return;
    auto* evt = Q_NEW(ReconfigureEvt, RECONFIGURE_SIG);
    evt->n_configs = 0;
    for (const auto& cfg : configs) {
        if (evt->n_configs >= ReconfigureEvt::MAX_CONFIGS) break;
        auto& e          = evt->entries[evt->n_configs++];
        e.id               = cfg.id;
        e.logic_positive   = cfg.logic_positive;
        e.detection_always = cfg.detection_always;
        e.n_linked = 0;
        for (int out : cfg.linked_outputs) {
            if (e.n_linked < ReconfigureEvt::MAX_LINKED)
                e.linked_outputs[e.n_linked++] = out;
        }
    }
    s_edgeDetector->post_(evt, nullptr);
}

// ── Server thread ─────────────────────────────────────────────────────────────

static std::atomic<bool> s_running{false};
static std::thread       s_thread;

static void server_loop(uint16_t port) {
    char addr[32];
    std::snprintf(addr, sizeof(addr), "http://0.0.0.0:%u", port);

    struct mg_mgr mgr;
    mg_mgr_init(&mgr);
    mg_http_listen(&mgr, addr, http_fn, NULL);
    std::printf("[HttpServer] listening on %s\n", addr);

    while (s_running.load()) {
        mg_mgr_poll(&mgr, 100);       // procesa red: acepta conexiones, HTTP request/response, WS recv
        push_if_pending(&mgr);        // push WS saliente si WsPublisher activó push_pending
    }

    mg_mgr_free(&mgr);
}

// ── Public API ────────────────────────────────────────────────────────────────

void HttpServer::start(uint16_t port, QP::QActive* edgeDetector) {
    s_edgeDetector = edgeDetector;
    s_running = true;
    s_thread  = std::thread(server_loop, port);
}

void HttpServer::stop() {
    s_running = false;
    if (s_thread.joinable()) s_thread.join();
}
