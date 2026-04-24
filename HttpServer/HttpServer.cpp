#include "HttpServer.h"
#include "../DigitalEdgeDetector/SharedState.h"
#include "../RemoteIO/RemoteIOState.h"
#include "../signals.h"
#include <thread>
#include <atomic>
#include <string>
#include <cstdio>
#include <cstring>

extern "C" {
#include "../mongoose/mongoose.h"
}

// HTML auto-generat des de web/index.html (veure web/gen_html_header.sh)
#include "../web/index_html.h"

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
    if (!se.push_pending.load()) return;
    se.push_pending.store(false);

    std::unordered_map<int, bool> inputs, outputs;
    std::unordered_map<int, int>  counts;
    std::vector<int>              edges;
    {
        std::lock_guard<std::mutex> lk(se.mtx);
        inputs   = se.inputs;
        outputs  = se.outputs;
        edges    = se.last_edges;
        counts   = se.edge_counts;
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
                std::lock_guard<std::mutex> lk(se.mtx);
                inputs   = se.inputs;
                outputs  = se.outputs;
                counts   = se.edge_counts;
            }
            std::string msg = build_ws_msg(inputs, outputs, {}, counts);
            mg_ws_send(c, msg.c_str(), msg.size(), WEBSOCKET_OP_TEXT);

        } else if (mg_match(hm->uri, mg_str("/configs"), NULL)
                   && mg_match(hm->method, mg_str("GET"), NULL)) {
            std::string body;
            {
                std::lock_guard<std::mutex> lk(se.mtx);
                body = "[";
                for (std::size_t i = 0; i < se.configs.size(); ++i) {
                    const auto& cfg = se.configs[i];
                    if (i > 0) body += ",";
                    body += "{\"id\":"               + std::to_string(cfg.id);
                    body += ",\"logic_positive\":"   + std::string(cfg.logic_positive   ? "true" : "false");
                    body += ",\"detection_always\":" + std::string(cfg.detection_always ? "true" : "false");
                    body += ",\"linked_outputs\":"   + int_vec_to_json(cfg.linked_outputs) + "}";
                }
                body += "]";
            }
            mg_http_reply(c, 200, "Content-Type: application/json\r\n", "%s", body.c_str());

        } else if (mg_match(hm->uri, mg_str("/configs"), NULL)
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
                { int llen = 0;
                  int loff = mg_json_get(elem, "$.linked_outputs", &llen);
                  if (loff >= 0) {
                      struct mg_str linked = { elem.buf + loff, (size_t)llen };
                      for (int k = 0; k < ReconfigureEvt::MAX_LINKED; ++k) {
                          char kpath[8];
                          std::snprintf(kpath, sizeof(kpath), "$[%d]", k);
                          long out_id = mg_json_get_long(linked, kpath, -1);
                          if (out_id < 0) break;
                          cfg.linked_outputs.push_back((int)out_id);
                      }
                  }
                }
                allConfigs.push_back(cfg);
            }
            if (allConfigs.empty()) { mg_http_reply(c, 400, "", "empty or invalid array\n"); return; }
            post_reconfigure(allConfigs);
            mg_http_reply(c, 200, "Content-Type: application/json\r\n", "{}");

        } else if (mg_match(hm->uri, mg_str("/"), NULL)) {
            // Servir HTML sin pasar por printf (evita interpretar % del CSS)
            mg_printf(c, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nCache-Control: no-cache\r\nContent-Length: %u\r\n\r\n",
                      (unsigned)std::strlen(s_html));
            mg_send(c, s_html, std::strlen(s_html));

        } else {
            mg_http_reply(c, 404, "", "Not found\n");
        }

    } else if (ev == MG_EV_WS_MSG) {
        auto* wm = static_cast<struct mg_ws_message*>(ev_data);
        if ((wm->flags & 0xF) != WEBSOCKET_OP_TEXT) return;

        std::unordered_map<int, bool> inputs, outputs;
        int ilen = 0, olen = 0;
        int ioff = mg_json_get(wm->data, "$.inputs",  &ilen);
        int ooff = mg_json_get(wm->data, "$.outputs", &olen);

        if (ioff > 0) parse_bool_object({wm->data.buf + ioff, (size_t)ilen}, inputs);
        if (ooff > 0) parse_bool_object({wm->data.buf + ooff, (size_t)olen}, outputs);

        if (!inputs.empty() || !outputs.empty()) {
            std::lock_guard<std::mutex> lk(remoteIO.mtx);
            for (auto const& [id, v] : inputs)  remoteIO.inputs[id]  = v;
            for (auto const& [id, v] : outputs) remoteIO.outputs[id] = v;
        }
    }
}

// ── Helper para PUT /config ──────────────────────────────────────────────────

static void post_reconfigure(const std::vector<InputConfig>& configs) {
    if (!s_edgeDetector) return;
    {
        std::lock_guard<std::mutex> lk(remoteIO.mtx);
        remoteIO.inputs.clear();
        remoteIO.outputs.clear();
        for (const auto& cfg : configs) {
            remoteIO.inputs[cfg.id] = false;
            for (int out_id : cfg.linked_outputs)
                remoteIO.outputs[out_id] = false;
        }
    }
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
