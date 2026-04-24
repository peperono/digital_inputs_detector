#pragma once
#include <mutex>
#include <unordered_map>

struct RemoteIOState {
    std::mutex                    mtx;
    std::unordered_map<int, bool> inputs;
    std::unordered_map<int, bool> outputs;
};

extern RemoteIOState remoteIO;
