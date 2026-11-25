#include "TestHelper.h"
#include "Core/InputKeyNames.h"
#include <algorithm>
#include <set>
#include <string>
#include <vector>

namespace {

bool Has(const std::vector<std::string>& names, const std::string& name) {
    return std::find(names.begin(), names.end(), name) != names.end();
}

}

TEST(new_keys_are_registered_with_unique_names_and_codes) {
    std::vector<std::string> names = GetInputKeyNames();
    CHECK(Has(names, "ANY"));
    CHECK(Has(names, "MOUSE_WHEEL_UP"));
    CHECK(Has(names, "MOUSE_WHEEL_DOWN"));

    std::set<std::string> uniqueNames(names.begin(), names.end());
    CHECK(uniqueNames.size() == names.size());

    std::set<int> codes;
    for (const std::string& name : names) {
        int code = InputKeyNameToCode(name);
        CHECK_MSG(codes.insert(code).second, name);
        CHECK_EQ_STR(InputKeyCodeToName(code), name);
    }
}

TEST(buttons_are_only_keys_and_buttons) {
    const char* buttons[] = { "A", "Z", "0", "SPACE", "F12", "LEFT_SHIFT", "LEFT_MOUSE", "RIGHT_MOUSE", "MIDDLE_MOUSE",
                              "GAMEPAD_FACE_UP", "GAMEPAD_DPAD_LEFT", "GAMEPAD_SHOULDER_R", "GAMEPAD_SPECIAL_L",
                              "GAMEPAD_RIGHT_THUMB" };
    for (const char* name : buttons) CHECK_MSG(InputKeyIsButton(name), name);

    const char* notButtons[] = { "ANY", "MOUSE_WHEEL", "MOUSE_WHEEL_UP", "MOUSE_WHEEL_DOWN", "MOUSE_X", "MOUSE_Y",
                                 "MOUSE_UP", "MOUSE_DOWN", "MOUSE_LEFT", "MOUSE_RIGHT", "GAMEPAD_LEFT_STICK_X",
                                 "GAMEPAD_RIGHT_STICK_Y", "GAMEPAD_LEFT_STICK_UP", "GAMEPAD_RIGHT_STICK_RIGHT",
                                 "GAMEPAD_TRIGGER_L", "GAMEPAD_TRIGGER_R" };
    for (const char* name : notButtons) CHECK_MSG(!InputKeyIsButton(name), name);
}

TEST(unknown_names_are_never_buttons) {
    CHECK(!InputKeyIsButton(""));
    CHECK(!InputKeyIsButton("NOT_A_KEY"));
    CHECK(!InputKeyIsButton("left_mouse"));
    CHECK(!InputKeyIsButton(std::string("A\0B", 3)));
    CHECK(!InputKeyIsButton(std::string(100000, 'x')));
}

TEST(any_and_wheel_never_claim_a_device) {
    CHECK(!InputKeyClaimsDevice("ANY"));
    CHECK(!InputKeyClaimsDevice("MOUSE_WHEEL"));
    CHECK(!InputKeyClaimsDevice("MOUSE_WHEEL_UP"));
    CHECK(!InputKeyClaimsDevice("MOUSE_WHEEL_DOWN"));
    CHECK(!InputKeyClaimsDevice("MOUSE_X"));
    CHECK(!InputKeyClaimsDevice("GAMEPAD_LEFT_STICK_X"));
    CHECK(!InputKeyClaimsDevice("GAMEPAD_TRIGGER_L"));
}

TEST(existing_claiming_keys_still_claim) {
    CHECK(InputKeyClaimsDevice("A"));
    CHECK(InputKeyClaimsDevice("SPACE"));
    CHECK(InputKeyClaimsDevice("LEFT_MOUSE"));
    CHECK(InputKeyClaimsDevice("MOUSE_UP"));
    CHECK(InputKeyClaimsDevice("GAMEPAD_FACE_DOWN"));
    CHECK(InputKeyClaimsDevice("GAMEPAD_LEFT_STICK_UP"));
}

TEST(claim_scan_set_matches_the_old_analog_rule_except_the_new_keys) {
    for (const std::string& name : GetInputKeyNames()) {
        bool oldRule = !InputKeyIsAnalog(name);
        bool isNew = name == "ANY" || name == "MOUSE_WHEEL_UP" || name == "MOUSE_WHEEL_DOWN";
        CHECK_MSG(InputKeyClaimsDevice(name) == (oldRule && !isNew), name);
    }
}

TEST(any_belongs_to_claimed_devices_only) {
    int pad = -1;
    CHECK(InputKeyBelongsToDevice("ANY", "Keyboard+Mouse", pad));
    CHECK(InputKeyBelongsToDevice("ANY", "Gamepad 0", pad));
    CHECK(InputKeyBelongsToDevice("ANY", "Gamepad 3", pad));
    CHECK(!InputKeyBelongsToDevice("ANY", "Unclaimed", pad));
    CHECK(!InputKeyBelongsToDevice("ANY", "", pad));
    CHECK(!InputKeyBelongsToDevice("ANY", "Nonsense", pad));
}

TEST(wheel_keys_belong_to_keyboard_and_mouse_only) {
    int pad = -1;
    CHECK(InputKeyBelongsToDevice("MOUSE_WHEEL_UP", "Keyboard+Mouse", pad));
    CHECK(InputKeyBelongsToDevice("MOUSE_WHEEL_DOWN", "Keyboard+Mouse", pad));
    CHECK(!InputKeyBelongsToDevice("MOUSE_WHEEL_UP", "Gamepad 0", pad));
    CHECK(!InputKeyBelongsToDevice("MOUSE_WHEEL_DOWN", "Unclaimed", pad));
}

TEST(idle_input_reports_nothing) {
    float axis = 5.0f;
    CHECK(!PollInputKey("ANY", axis));
    CHECK(axis == 0.0f);
    axis = 5.0f;
    CHECK(!PollInputKey("MOUSE_WHEEL_UP", axis));
    CHECK(axis == 0.0f);
    axis = 5.0f;
    CHECK(!PollInputKey("MOUSE_WHEEL_DOWN", axis));
    CHECK(axis == 0.0f);

    std::vector<std::string> down = { "stale", "entries" };
    CollectDownInputButtons(down);
    CHECK(down.empty());
}

TEST_MAIN()
