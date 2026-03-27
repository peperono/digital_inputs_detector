#pragma once
#include <vector>
#include <unordered_map>

struct InputConfig {
    int id;
    bool logic_positive;   // true = positive (rising edge), false = negative (falling edge)
    bool detection_always; // true = 24h, false = only when linked output is active
    std::vector<int> linked_outputs;
};

struct DigitalEdgeDetector {
    void configure(const std::vector<InputConfig>& configs);
    // Returns IDs of inputs that generated an activation pulse
    std::vector<int> process(const std::unordered_map<int, bool>& input_states,
                             const std::unordered_map<int, bool>& output_states);

private:
    std::vector<InputConfig> configs_;
    std::unordered_map<int, bool> prev_states_;

    bool detection_enabled(const InputConfig& cfg,
                           const std::unordered_map<int, bool>& output_states) const;
};
