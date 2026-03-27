#include "DigitalInputDetector.h"

void DigitalInputDetector::configure(const std::vector<InputConfig>& configs) {
    configs_ = configs;
    prev_states_.clear();
}

std::vector<int> DigitalInputDetector::process(
    const std::unordered_map<int, bool>& input_states,
    const std::unordered_map<int, bool>& output_states)
{
    std::vector<int> pulses;

    for (const auto& cfg : configs_) {
        auto it = input_states.find(cfg.id);
        if (it == input_states.end()) continue;

        bool current = it->second;
        bool prev    = prev_states_.count(cfg.id) ? prev_states_.at(cfg.id) : current;

        bool rising_edge  = !prev && current;
        bool falling_edge =  prev && !current;
        bool edge_detected = cfg.logic_positive ? rising_edge : falling_edge;

        if (edge_detected && detection_enabled(cfg, output_states))
            pulses.push_back(cfg.id);

        prev_states_[cfg.id] = current;
    }

    return pulses;
}

bool DigitalInputDetector::detection_enabled(
    const InputConfig& cfg,
    const std::unordered_map<int, bool>& output_states) const
{
    if (cfg.detection_always) return true;

    for (int out_id : cfg.linked_outputs) {
        auto it = output_states.find(out_id);
        if (it != output_states.end()) {
            bool state = it->second;
            if (state) return true;
        }
    }
    return false;
}
