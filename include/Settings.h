#pragma once
#include "SimpleIni.h"

using namespace SKSE;
using namespace SKSE::log;

class Settings {
public:
    [[nodiscard]] static Settings* GetSingleton();
    
    // Simplified struct for easy copying/overriding
    struct ClimbingSettings {
        float fStaminaCostMove{0.30f};
        float fStaminaCostIdle{0.02f};
        float fStaminaOneHandCostMult{2.0f}; 
        float fStaminaMovementThreshold{10.0f};

        float fForceMulti{1.0f};
        float fRayDist{65.0f};
        float fMaxArmLength{120.0f}; 
        float fGrabSmoothing{0.2f};

        float fThrowMult{1.2f};
        float fThrowReleaseThreshold{200.0f};
        float fThrowTimeWindow{0.5f}; // Time in seconds to "remember" peak velocity
        float fMaxFlingVelocity{800.0f};

        float fMaxVelocity{1500.0f}; 
        float fMotionSmoothing{0.4f}; // [0.0 - 1.0]. Lower = Less Jitter/More Lag.
        bool bEnableHaptics{true};
        bool bEnableStamina{true};
        bool bEnableWholeMod{true};
        bool bDisableFallDamage{true}; // New option
    };

    void Load();
    void ApplyRace(const std::string& raceName); // Apply overrides

    ClimbingSettings defaultSettings; // The base settings from [Climbing]
    ClimbingSettings activeSettings;  // The settings currently in use (Base + Race)
    
    // Map of Race EditorID -> Settings Override (Partial or Full)
    std::map<std::string, ClimbingSettings> raceOverrides;

private:
    Settings() = default;
    Settings(const Settings&) = delete;
    Settings(Settings&&) = delete;
    ~Settings() = default;

    Settings& operator=(const Settings&) = delete;
    Settings& operator=(Settings&&) = delete;
};

// Global State Variables (Needed for OnFrame loop)
extern int64_t iFrameCount;
extern int64_t iLastPressGrip;
extern std::chrono::steady_clock::time_point last_time;