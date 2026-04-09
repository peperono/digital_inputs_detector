#pragma once
#include <vector>

struct InputConfig {
    int  id;
    bool logic_positive;   // true  → rising edge (false→true) triggers pulse
    bool detection_always; // true  → always active; false → only when a linked output is ON
    std::vector<int> linked_outputs;
};
