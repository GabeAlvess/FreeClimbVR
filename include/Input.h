#pragma once
#include "RE/Skyrim.h"
#include <chrono>
#include <atomic>

class InputManager : public RE::BSTEventSink<RE::InputEvent*> {
public:
    static InputManager* GetSingleton();

    void Register();
    
    // Status Accessors
    bool IsLeftGripPressed();
    bool IsRightGripPressed();
    bool IsSneaking(); 

protected:
    RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>* a_source) override;

private:
    std::chrono::steady_clock::time_point _lastLeftPress;
    std::chrono::steady_clock::time_point _lastRightPress;
    std::chrono::steady_clock::time_point _lastSneakPress;

    std::atomic<bool> _leftGrip{ false };
    std::atomic<bool> _rightGrip{ false };
};
