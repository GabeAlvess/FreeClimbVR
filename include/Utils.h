#pragma once
#include <random>
#include <sstream>
#include "settings.h"

using namespace SKSE;
using namespace SKSE::log;

// Core Form/Global Lookups
uint32_t GetBaseFormID(uint32_t formId);
uint32_t GetFullFormID(const uint8_t modIndex, uint32_t formLower);
uint32_t GetFullFormID_ESL(const uint8_t modIndex, const uint16_t esl_index, uint32_t formLower);

// Inputs (Globals) - REMOVED

// Utilities
std::string formatNiPoint3(RE::NiPoint3& pos);

// Haptics
void vibrateController(int hapticFrame, int length, bool isLeft);

// Positions & Physics
// Positions & Physics
RE::NiPoint3 GetPlayerHandPos(bool isLeft, RE::Actor* player);
RE::NiPoint3 Quad2Velo(RE::hkVector4& a_velocity);

// Raycast Result
struct ClimbHitData {
    bool hit{ false };
    RE::NiPoint3 normal;
    RE::TESObjectREFR* refr{ nullptr };
};

// Collision Detection
ClimbHitData CheckClimbCollision(RE::Actor* player, bool isLeft, float rayDist);
bool IsIce(RE::TESObjectREFR* ref);
bool IsClimbingTool(RE::Actor* player, bool isLeft);