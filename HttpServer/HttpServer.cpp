#include "HttpServer.h"
#include "../SharedState.h"
#include <thread>
#include <atomic>
#include <string>
#include <cstdio>

extern "C" {
#include "../mongoose/mongoose.h"
}

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

static std::string int_vec_to_json(const std::vector<int>& v) {
    std::string s = "[";
    for (std::size_t i = 0; i < v.size(); ++i) {
        if (i > 0) s += ",";
        s += std::to_string(v[i]);
    }
    s += "]";
    return s;
}

// ── Mongoose event handler ────────────────────────────────────────────────────

static void http_fn(struct mg_connection* c, int ev, void* ev_data) {
    if (ev != MG_EV_HTTP_MSG) return;

    auto* hm = static_cast<struct mg_http_message*>(ev_data);

    if (mg_match(hm->uri, mg_str("/state"), NULL)) {
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

    } else {
        mg_http_reply(c, 404, "", "Not found\n");
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
