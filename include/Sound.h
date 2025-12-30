#pragma once
#include <RE/Skyrim.h>

namespace Sound {
    // Play a climbing impact sound based on the surface material
    void PlayClimbSound(RE::TESObjectREFR* surfaceRef, RE::Actor* player);
}
