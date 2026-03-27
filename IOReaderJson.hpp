#pragma once
#include "IOStateMonitor/IOStateMonitor.h"
#include <nlohmann/json.hpp>
#include <mutex>
#include <string>
#include <unordered_map>

// ── JsonIOSource ──────────────────────────────────────────────────────────────
// Thread-safe, JSON-backed IO source for IOStateMonitor.
//
// The driver/test thread calls update() with a JSON snapshot.
// The QP thread reads the state through the IOReader returned by reader().
//
// JSON format:
//   { "inputs":  { "1": true,  "2": false },
//     "outputs": { "10": true, "11": false } }
//
// Channel IDs are string keys; states are JSON booleans.

class JsonIOSource {
public:
    // Replace the current snapshot (callable from any thread).
    void update(std::string json_str) {
        std::lock_guard<std::mutex> lk(mtx_);
        json_str_ = std::move(json_str);
    }

    // Returns an IOReader that always reflects the latest update() call.
    // Call once and pass the result to IOStateMonitor's constructor.
    IOReader reader() {
        return [this](std::unordered_map<int, bool>& inputs,
                      std::unordered_map<int, bool>& outputs)
        {
            std::lock_guard<std::mutex> lk(mtx_);
            if (json_str_.empty()) return;

            auto j = nlohmann::json::parse(json_str_,
                                           /*cb=*/nullptr,
                                           /*exceptions=*/false);
            if (!j.is_object()) return;

            inputs.clear();
            for (auto& [k, v] : j.value("inputs",  nlohmann::json::object()).items())
                inputs[std::stoi(k)] = v.get<bool>();

            outputs.clear();
            for (auto& [k, v] : j.value("outputs", nlohmann::json::object()).items())
                outputs[std::stoi(k)] = v.get<bool>();
        };
    }

private:
    std::mutex  mtx_;
    std::string json_str_;
};
