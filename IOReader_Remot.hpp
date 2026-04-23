#pragma once
#include "DigitalEdgeDetector/DigitalEdgeDetector.h"
#include "RemoteIOState.h"
#include <mutex>

inline IOReader makeRemoteReader() {
    return [](std::unordered_map<int, bool>& inputs,
              std::unordered_map<int, bool>& outputs) {
        std::lock_guard<std::mutex> lk(remoteIO.mtx);
        inputs  = remoteIO.inputs;
        outputs = remoteIO.outputs;
    };
}
