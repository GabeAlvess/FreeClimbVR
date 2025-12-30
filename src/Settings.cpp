#include "settings.h"
#include <fstream>
#include <string> // For std::string
#include <map>    // For std::map

// Global State Definitions
int64_t iFrameCount = 0;
int64_t iLastPressGrip = 0;
std::chrono::steady_clock::time_point last_time;

// Helper function to load a section into ClimbingSettings, using existing values as defaults
void LoadSection(CSimpleIniA& a_ini, const char* section, Settings::ClimbingSettings& out) {
    out.fStaminaCostMove = (float)a_ini.GetDoubleValue(section, "fStaminaCostMove", out.fStaminaCostMove);
    out.fStaminaCostIdle = (float)a_ini.GetDoubleValue(section, "fStaminaCostIdle", out.fStaminaCostIdle);
    out.fStaminaOneHandCostMult = (float)a_ini.GetDoubleValue(section, "fStaminaOneHandCostMult", out.fStaminaOneHandCostMult);
    out.fStaminaMovementThreshold = (float)a_ini.GetDoubleValue(section, "fStaminaMovementThreshold", out.fStaminaMovementThreshold);

    out.fForceMulti = (float)a_ini.GetDoubleValue(section, "fForceMulti", out.fForceMulti);
    out.fRayDist = (float)a_ini.GetDoubleValue(section, "fRayDist", out.fRayDist);
    out.fMaxArmLength = (float)a_ini.GetDoubleValue(section, "fMaxArmLength", out.fMaxArmLength);
    out.fGrabSmoothing = (float)a_ini.GetDoubleValue(section, "fGrabSmoothing", out.fGrabSmoothing);

    out.fThrowMult = (float)a_ini.GetDoubleValue(section, "fThrowMult", out.fThrowMult);
    out.fThrowReleaseThreshold = (float)a_ini.GetDoubleValue(section, "fThrowReleaseThreshold", out.fThrowReleaseThreshold);
    out.fThrowTimeWindow = (float)a_ini.GetDoubleValue(section, "fThrowTimeWindow", out.fThrowTimeWindow);
    out.fMaxFlingVelocity = (float)a_ini.GetDoubleValue(section, "fMaxFlingVelocity", out.fMaxFlingVelocity);
    
    out.fMaxVelocity = (float)a_ini.GetDoubleValue(section, "fMaxVelocity", out.fMaxVelocity);
    out.fMotionSmoothing = (float)a_ini.GetDoubleValue(section, "fMotionSmoothing", out.fMotionSmoothing);

    out.bEnableHaptics = a_ini.GetBoolValue(section, "bEnableHaptics", out.bEnableHaptics);
    out.bEnableStamina = a_ini.GetBoolValue(section, "bEnableStamina", out.bEnableStamina);
    out.bDisableFallDamage = a_ini.GetBoolValue(section, "bDisableFallDamage", out.bDisableFallDamage);
    out.bEnableWholeMod = a_ini.GetBoolValue(section, "bEnableWholeMod", out.bEnableWholeMod);
}

Settings* Settings::GetSingleton() {
    static Settings singleton;
    return &singleton;
}

void Settings::Load() {
    CSimpleIniA ini;
    ini.SetUnicode();
    
    const char* path = "Data/SKSE/Plugins/FreeClimbVR_Settings.ini";

    // Initialize default settings with hardcoded values first
    // These will be used if the INI file or specific keys are missing
    defaultSettings.fStaminaCostMove = 0.30f;
    defaultSettings.fStaminaCostIdle = 0.02f;
    defaultSettings.fStaminaOneHandCostMult = 2.0f;
    defaultSettings.fMaxArmLength = 120.0f;
    defaultSettings.fMaxVelocity = 1500.0f;
    defaultSettings.fMaxFlingVelocity = 800.0f;
    defaultSettings.fGrabSmoothing = 0.2f;
    defaultSettings.fThrowMult = 1.5f;
    defaultSettings.fForceMulti = 1.1f;
    defaultSettings.fRayDist = 65.0f;
    defaultSettings.fThrowReleaseThreshold = 180.0f;
    defaultSettings.fThrowTimeWindow = 0.6f;
    defaultSettings.fStaminaMovementThreshold = 10.0f;
    defaultSettings.bEnableHaptics = true;
    defaultSettings.bEnableStamina = true;
    defaultSettings.bDisableFallDamage = true;
    defaultSettings.bEnableWholeMod = true;

    // Load the INI file
    SI_Error status = ini.LoadFile(path);

    // Load Base "Climbing" section into defaultSettings
    LoadSection(ini, "Climbing", defaultSettings);
    activeSettings = defaultSettings; // Start with base settings

    // Save back to ensure defaults are written if missing or file was new
    // This also writes comments for new entries
    ini.SetDoubleValue("Climbing", "fStaminaCostMove", defaultSettings.fStaminaCostMove, "# Stamina cost per frame while moving/climbing");
    ini.SetDoubleValue("Climbing", "fStaminaCostIdle", defaultSettings.fStaminaCostIdle, "# Stamina cost per frame while just hanging");
    ini.SetDoubleValue("Climbing", "fStaminaOneHandCostMult", defaultSettings.fStaminaOneHandCostMult, "# Stamina multiplier when using only 1 hand");
    ini.SetDoubleValue("Climbing", "fMaxArmLength", defaultSettings.fMaxArmLength, "# Maximum distance between hand and grab point before auto-release");
    ini.SetDoubleValue("Climbing", "fMaxVelocity", defaultSettings.fMaxVelocity, "# Safety limit for velocity to avoid physics explosions");
    ini.SetDoubleValue("Climbing", "fMaxFlingVelocity", defaultSettings.fMaxFlingVelocity, "# Max vertical velocity for flung jumps");
    ini.SetDoubleValue("Climbing", "fGrabSmoothing", defaultSettings.fGrabSmoothing, "# Time in seconds to smooth/dampen the grab impact");
    ini.SetDoubleValue("Climbing", "fThrowMult", defaultSettings.fThrowMult, "# Multiplier for vertical velocity when throwing/flinging");
    ini.SetDoubleValue("Climbing", "fForceMulti", defaultSettings.fForceMulti, "# General climbing speed multiplier");
    ini.SetDoubleValue("Climbing", "fRayDist", defaultSettings.fRayDist, "# Max distance from hand to surface to grab");
    ini.SetDoubleValue("Climbing", "fThrowReleaseThreshold", defaultSettings.fThrowReleaseThreshold, "# Vertical velocity threshold to auto-release hands");
    ini.SetDoubleValue("Climbing", "fThrowTimeWindow", defaultSettings.fThrowTimeWindow, "# Time window (seconds) to remember peak velocity for fling");
    ini.SetDoubleValue("Climbing", "fStaminaMovementThreshold", defaultSettings.fStaminaMovementThreshold, "# Velocity threshold to consider 'Moving' vs 'Idle'");
    ini.SetBoolValue("Climbing", "bEnableHaptics", defaultSettings.bEnableHaptics, "# Enable controller vibration on grab");
    ini.SetBoolValue("Climbing", "bEnableStamina", defaultSettings.bEnableStamina, "# Enable stamina drain system");
    ini.SetBoolValue("Climbing", "bEnableWholeMod", defaultSettings.bEnableWholeMod, "# Master switch for the mod");

    // Load Race Overrides
    // Standard Skyrim Races
    const char* races[] = {"NordRace", "KhajiitRace", "ArgonianRace", "OrcRace", "WoodElfRace", "HighElfRace", "DarkElfRace", "BretonRace", "RedguardRace", "ImperialRace"};
    
    for (const char* r : races) {
        std::string sec = "Race_" + std::string(r);
        if (ini.GetSection(sec.c_str())) {
            // Found override section
            ClimbingSettings raceSet = defaultSettings; // inherit base settings
            LoadSection(ini, sec.c_str(), raceSet);     // apply overrides from INI
            raceOverrides[r] = raceSet;                 // store
            log::info("Loaded Race Override: {}", r);
        }
    }

    // Save the INI file back to disk, ensuring any new defaults or comments are written
    ini.SaveFile(path);
}

void Settings::ApplyRace(const std::string& raceName) {
    static std::string lastApplied = "";
    if (lastApplied == raceName) return; // Prevent spam

    if (raceOverrides.count(raceName)) {
        activeSettings = raceOverrides[raceName];
        log::info("Applied settings for race: {}", raceName);
        RE::DebugNotification(("VRClimbing Profile: " + raceName).c_str());
        lastApplied = raceName;
    } else {
        if (lastApplied != "Default") {
             activeSettings = defaultSettings;
             // log::info("Applied default settings (Race not found: {})", raceName);
             lastApplied = "Default";
        }
    }
}