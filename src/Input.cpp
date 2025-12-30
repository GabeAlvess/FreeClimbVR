#include "Input.h"

using namespace RE;

InputManager* InputManager::GetSingleton() {
    static InputManager singleton;
    return &singleton;
}

void InputManager::Register() {
    auto deviceManager = BSInputDeviceManager::GetSingleton();
    if (deviceManager) {
        deviceManager->AddEventSink(this);
        SKSE::log::info("Registered Input Event Sink");
    } else {
        SKSE::log::error("Failed to get BSInputDeviceManager! Input will not work.");
    }
}

bool InputManager::IsLeftGripPressed() {
    auto now = std::chrono::steady_clock::now();
    auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(now - _lastLeftPress).count();
    return dur < 100; // 100ms timeout
}

bool InputManager::IsRightGripPressed() {
     auto now = std::chrono::steady_clock::now();
    auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(now - _lastRightPress).count();
    return dur < 100; 
}

bool InputManager::IsSneaking() {
    auto now = std::chrono::steady_clock::now();
    auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(now - _lastSneakPress).count();
    return dur < 250; // 250ms window
}

RE::BSEventNotifyControl InputManager::ProcessEvent(InputEvent* const* a_event, BSTEventSource<InputEvent*>* a_source) {
    if (!a_event) return BSEventNotifyControl::kContinue;

    for (auto event = *a_event; event; event = event->next) {
        if (event->GetEventType() != INPUT_EVENT_TYPE::kButton) continue;

        auto buttonEvent = event->AsButtonEvent();
        if (!buttonEvent) continue;

        // Check User Events (Like Sneak)
        if (buttonEvent->Value() > 0.0f || buttonEvent->HeldDuration() > 0.0f) {
             auto userEvent = buttonEvent->QUserEvent();
             if (!userEvent.empty()) {
                 // SKSE::log::info("UserEvent: {}", userEvent.c_str()); 
             }
             if (userEvent == "Sneak") {
                 _lastSneakPress = std::chrono::steady_clock::now();
             }
        }

        auto device = buttonEvent->GetDevice();
        auto idCode = buttonEvent->GetIDCode();

        // Check for Grip Button (ID 2).
        // We ignore IDs 33/34 to prevent duplicate event processing.
        bool isGripID = (idCode == 2); 
        
        if (isGripID) {
             int devInt = (int)device;
             
             // Left Hand Detection
             // Device 1: Standard Left Controller
             // Device 6: Detected as Left in some VR configs (Fix for inverted controls)
             if (devInt == 1 || devInt == 6) {
                 _lastLeftPress = std::chrono::steady_clock::now();
             }
             
             // Right Hand Detection
             // Device 2: Standard Right Controller
             // Device 5: Detected as Right in some VR configs (Fix for inverted controls)
             if (devInt == 2 || devInt == 5) {
                 _lastRightPress = std::chrono::steady_clock::now();
             }
        }
    }

    return BSEventNotifyControl::kContinue;
}
