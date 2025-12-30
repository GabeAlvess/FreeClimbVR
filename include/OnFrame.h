#pragma once
#include <RE/Skyrim.h>
#include "Utils.h"
#include "Player.h"

using namespace SKSE;
using namespace SKSE::log;

extern bool bDisableSpark;

namespace ZacOnFrame {

    // Main Loops
    void InstallFrameHook();
    void OnFrameUpdate();
    void ClimbMain(float dt);
    
    // Hooks
    void HookSetVelocity(RE::bhkCharProxyController* controller, const RE::hkVector4& a_velocity);
    
    // Stubs / Utilities
    bool IsNiPointZero(const RE::NiPoint3&);
    void CleanBeforeLoad();
    
    // Legacy Stubs to satisfy linker
    void TimeSlowEffect(RE::Actor*, int64_t, float);
    void StopTimeSlowEffect(RE::Actor*);
    RE::hkVector4 CalculatePushVector(RE::NiPoint3 sourcePos, RE::NiPoint3 targetPos, bool isEnemy, float speed);

    // Relocations
    static REL::Relocation<decltype(OnFrameUpdate)> _OnFrame;
    static REL::Relocation<decltype(HookSetVelocity)> _SetVelocity;

}
