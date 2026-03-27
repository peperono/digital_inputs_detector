// Compile & run (MSVC):  cl /EHsc /std:c++17 test_DigitalEdgeDetector.cpp DigitalEdgeDetector.cpp && test_DigitalEdgeDetector.exe
// Compile & run (GCC):   g++ -std=c++17 -o test_dd test_DigitalEdgeDetector.cpp DigitalEdgeDetector.cpp && ./test_dd

#include "DigitalEdgeDetector.h"
#include <cassert>
#include <cstdio>

// ── helpers ──────────────────────────────────────────────────────────────────

static InputConfig make_cfg(int id,
                             bool logic_positive,
                             bool detection_always,
                             std::vector<int> linked = {})
{
    return InputConfig{id, logic_positive, detection_always, std::move(linked)};
}

static std::unordered_map<int, bool> states(std::initializer_list<std::pair<const int, bool>> il)
{
    return std::unordered_map<int, bool>(il);
}

// ── tests ─────────────────────────────────────────────────────────────────────

// Rising edge (logic_positive=true) triggers a pulse
static void test_rising_edge_triggers()
{
    DigitalEdgeDetector d;
    d.configure({make_cfg(1, /*logic_positive=*/true, /*always=*/true)});

    // First call: prev is initialised to current (false), no edge
    auto r1 = d.process(states({{1, false}}), {});
    assert(r1.empty());

    // Transition false → true: rising edge → pulse
    auto r2 = d.process(states({{1, true}}), {});
    assert(r2.size() == 1 && r2[0] == 1);

    // Stays high: no new edge
    auto r3 = d.process(states({{1, true}}), {});
    assert(r3.empty());

    printf("PASS  test_rising_edge_triggers\n");
}

// Falling edge (logic_positive=false) triggers a pulse
static void test_falling_edge_triggers()
{
    DigitalEdgeDetector d;
    d.configure({make_cfg(1, /*logic_positive=*/false, /*always=*/true)});

    d.process(states({{1, true}}), {});           // establish prev = true

    // Transition true → false: falling edge → pulse
    auto r = d.process(states({{1, false}}), {});
    assert(r.size() == 1 && r[0] == 1);

    printf("PASS  test_falling_edge_triggers\n");
}

// Rising edge does NOT trigger for logic_positive=false
static void test_rising_edge_no_pulse_when_logic_negative()
{
    DigitalEdgeDetector d;
    d.configure({make_cfg(1, /*logic_positive=*/false, /*always=*/true)});

    d.process(states({{1, false}}), {});           // prev = false

    auto r = d.process(states({{1, true}}), {});   // rising edge, but wrong polarity
    assert(r.empty());

    printf("PASS  test_rising_edge_no_pulse_when_logic_negative\n");
}

// detection_always=false: no pulse when all linked outputs are inactive
static void test_no_pulse_when_outputs_inactive()
{
    DigitalEdgeDetector d;
    d.configure({make_cfg(1, true, /*always=*/false, {10, 11})});

    d.process(states({{1, false}}), states({{10, false}, {11, false}}));

    auto r = d.process(states({{1, true}}), states({{10, false}, {11, false}}));
    assert(r.empty());

    printf("PASS  test_no_pulse_when_outputs_inactive\n");
}

// detection_always=false: pulse fires when at least one linked output is active
static void test_pulse_when_one_output_active()
{
    DigitalEdgeDetector d;
    d.configure({make_cfg(1, true, /*always=*/false, {10, 11})});

    d.process(states({{1, false}}), states({{10, false}, {11, true}}));

    auto r = d.process(states({{1, true}}), states({{10, false}, {11, true}}));
    assert(r.size() == 1 && r[0] == 1);

    printf("PASS  test_pulse_when_one_output_active\n");
}

// Input absent from input_states map → no pulse, no crash
static void test_missing_input_is_ignored()
{
    DigitalEdgeDetector d;
    d.configure({make_cfg(1, true, true)});

    auto r = d.process(/*empty*/{}, {});
    assert(r.empty());

    printf("PASS  test_missing_input_is_ignored\n");
}

// configure() resets prev_states so a fresh rising edge is detected again
static void test_reconfigure_resets_state()
{
    DigitalEdgeDetector d;
    d.configure({make_cfg(1, true, true)});

    d.process(states({{1, true}}), {});   // prev becomes true
    d.process(states({{1, true}}), {});   // no edge

    // Reconfigure: prev_states cleared
    d.configure({make_cfg(1, true, true)});

    // First call after reconfigure: prev initialised to current (true), no edge
    auto r1 = d.process(states({{1, true}}), {});
    assert(r1.empty());

    // Now fall and rise again
    d.process(states({{1, false}}), {});
    auto r2 = d.process(states({{1, true}}), {});
    assert(r2.size() == 1 && r2[0] == 1);

    printf("PASS  test_reconfigure_resets_state\n");
}

// Multiple inputs independently detected in the same call
static void test_multiple_inputs_same_cycle()
{
    DigitalEdgeDetector d;
    d.configure({
        make_cfg(1, true,  true),
        make_cfg(2, false, true),
    });

    d.process(states({{1, false}, {2, true}}), {});  // establish prev

    // Rising on 1, falling on 2 — both should fire
    auto r = d.process(states({{1, true}, {2, false}}), {});
    assert(r.size() == 2);

    printf("PASS  test_multiple_inputs_same_cycle\n");
}

// ── main ──────────────────────────────────────────────────────────────────────

int main()
{
    test_rising_edge_triggers();
    test_falling_edge_triggers();
    test_rising_edge_no_pulse_when_logic_negative();
    test_no_pulse_when_outputs_inactive();
    test_pulse_when_one_output_active();
    test_missing_input_is_ignored();
    test_reconfigure_resets_state();
    test_multiple_inputs_same_cycle();

    printf("\nAll tests passed.\n");
    return 0;
}
