#include "HttpServer.h"
#include "../SharedState.h"
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
    #status { margin-bottom: 24px; font-size: 14px; }
    .ok  { color: #3fb950; }
    .err { color: #f85149; }
    .section { display: inline-block; vertical-align: top; margin-right: 32px; margin-bottom: 24px; }
    h2 { color: #8b949e; font-size: 13px; text-transform: uppercase; letter-spacing: 1px; margin-bottom: 8px; }
    table { border-collapse: collapse; min-width: 140px; }
    th, td { border: 1px solid #30363d; padding: 6px 16px; text-align: center; font-size: 14px; }
    th { background: #161b22; color: #8b949e; }
    .on  { color: #3fb950; font-weight: bold; }
    .off { color: #f85149; }
    #edges-log { background: #161b22; border: 1px solid #30363d; padding: 8px 12px;
                 height: 160px; overflow-y: auto; font-size: 13px; list-style: none; }
    #edges-log li { padding: 3px 0; border-bottom: 1px solid #21262d; color: #d2a679; }
    #edges-log li:last-child { border-bottom: none; }
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

  <script>
    const statusEl   = document.getElementById('status');
    const inputsTbl  = document.getElementById('tbl-inputs');
    const outputsTbl = document.getElementById('tbl-outputs');
    const edgesLog   = document.getElementById('edges-log');

    function updateInputsTable(tbl, inputs, counts) {
      while (tbl.rows.length > 1) tbl.deleteRow(1);
      Object.entries(inputs)
        .sort(([a], [b]) => Number(a) - Number(b))
        .forEach(([id, state]) => {
          const row  = tbl.insertRow();
          row.insertCell().textContent = id;
          const cell = row.insertCell();
          cell.textContent = state ? 'ON' : 'OFF';
          cell.className   = state ? 'on' : 'off';
          row.insertCell().textContent = counts[id] ?? 0;
        });
    }

    function updateOutputsTable(tbl, map) {
      while (tbl.rows.length > 1) tbl.deleteRow(1);
      Object.entries(map)
        .sort(([a], [b]) => Number(a) - Number(b))
        .forEach(([id, state]) => {
          const row  = tbl.insertRow();
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

    function connect() {
      const ws = new WebSocket('ws://' + location.host + '/ws');

      ws.onopen = () => {
        statusEl.textContent = '\u25CF Conectado';
        statusEl.className   = 'ok';
      };
      ws.onclose = () => {
        statusEl.textContent = '\u25CF Desconectado \u2014 reintentando...';
        statusEl.className   = 'err';
        setTimeout(connect, 2000);
      };
      ws.onerror = () => ws.close();
      ws.onmessage = ({ data }) => {
        const d = JSON.parse(data);
        if (d.inputs)  updateInputsTable(inputsTbl,  d.inputs, d.edge_counts || {});
        if (d.outputs) updateOutputsTable(outputsTbl, d.outputs);
        if (d.last_edges && d.last_edges.length > 0) addEdgeEntry(d.last_edges);
      };
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

// ── Cambios de estado → push WebSocket ───────────────────────────────────────

struct Snapshot {
    std::unordered_map<int, bool> inputs;
    std::unordered_map<int, bool> outputs;
    std::uint32_t                 edge_gen{0};
};

static Snapshot s_last;

static void push_if_changed(struct mg_mgr* mgr) {
    std::unordered_map<int, bool> inputs, outputs;
    std::unordered_map<int, int>  counts;
    std::vector<int>              edges;
    std::uint32_t                 gen;
    {
        std::lock_guard<std::mutex> lk(g_state.mtx);
        inputs  = g_state.inputs;
        outputs = g_state.outputs;
        edges   = g_state.last_edges;
        counts  = g_state.edge_counts;
        gen     = g_state.edge_gen;
    }

    bool io_changed   = (inputs != s_last.inputs) || (outputs != s_last.outputs);
    bool edge_changed = (gen    != s_last.edge_gen);
    if (!io_changed && !edge_changed) return;

    std::vector<int> edges_to_send = edge_changed ? edges : std::vector<int>{};
    std::string msg = build_ws_msg(inputs, outputs, edges_to_send, counts);

    for (struct mg_connection* c = mgr->conns; c != nullptr; c = c->next) {
        if (c->is_websocket) {
            mg_ws_send(c, msg.c_str(), msg.size(), WEBSOCKET_OP_TEXT);
        }
    }

    s_last.inputs   = std::move(inputs);
    s_last.outputs  = std::move(outputs);
    s_last.edge_gen = gen;
}

// ── Mongoose event handler ────────────────────────────────────────────────────

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

        } else if (mg_match(hm->uri, mg_str("/state"), NULL)) {
            std::string body;
            {
                std::lock_guard<std::mutex> lk(g_state.mtx);
                body  = "{\"inputs\":"  + bool_map_to_json(g_state.inputs);
                body += ",\"outputs\":" + bool_map_to_json(g_state.outputs) + "}";
            }
            mg_http_reply(c, 200, "Content-Type: application/json\r\n", "%s", body.c_str());

        } else if (mg_match(hm->uri, mg_str("/edges"), NULL)) {
            std::string body;
            {
                std::lock_guard<std::mutex> lk(g_state.mtx);
                body = "{\"last_edges\":" + int_vec_to_json(g_state.last_edges) + "}";
            }
            mg_http_reply(c, 200, "Content-Type: application/json\r\n", "%s", body.c_str());

        } else if (mg_match(hm->uri, mg_str("/"), NULL)) {
            // Servir HTML sin pasar por printf (evita interpretar % del CSS)
            mg_printf(c, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: %u\r\n\r\n",
                      (unsigned)std::strlen(s_html));
            mg_send(c, s_html, std::strlen(s_html));

        } else {
            mg_http_reply(c, 404, "", "Not found\n");
        }

    } else if (ev == MG_EV_WS_MSG) {
        // Servidor de solo lectura: ignorar mensajes entrantes
    }
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
        mg_mgr_poll(&mgr, 100);
        push_if_changed(&mgr);
    }

    mg_mgr_free(&mgr);
}

// ── Public API ────────────────────────────────────────────────────────────────

void HttpServer::start(uint16_t port) {
    s_running = true;
    s_thread  = std::thread(server_loop, port);
}

void HttpServer::stop() {
    s_running = false;
    if (s_thread.joinable()) s_thread.join();
}
