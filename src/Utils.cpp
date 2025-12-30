#include "Utils.h"
#include <RE/H/hkpWorld.h> 
#include <RE/H/hkpWorldRayCastOutput.h>
#include <RE/T/TESHavokUtilities.h>

using namespace SKSE;
using namespace SKSE::log;

std::string formatNiPoint3(RE::NiPoint3& pos) {
    std::ostringstream stream;
    stream << "(" << pos.x << ", " << pos.y << ", " << pos.z << ")";
    return stream.str();
}

uint32_t GetBaseFormID(uint32_t formId) { return formId & 0x00FFFFFF; }

uint32_t GetFullFormID(const uint8_t modIndex, uint32_t formLower) { return (modIndex << 24) | formLower; }

uint32_t GetFullFormID_ESL(const uint8_t modIndex, const uint16_t esl_index, uint32_t formLower) {
    return (modIndex << 24) | (esl_index << 12) | formLower;
}

RE::NiPoint3 GetPlayerHandPos(bool isLeft, RE::Actor* player) {
    auto playerCh = RE::PlayerCharacter::GetSingleton();
    if (!playerCh) return RE::NiPoint3();
    
    auto vrData = playerCh->GetVRNodeData();
    const auto weapon = isLeft ? vrData->NPCLHnd : vrData->NPCRHnd;
    const auto baseNode = vrData->UprightHmdNode;

    if (weapon && baseNode) {
        auto pos = weapon->world.translate - baseNode->world.translate;
        pos.z += 120.0f; 
        return pos;
    }
    return RE::NiPoint3();
}

RE::NiPoint3 Quad2Velo(RE::hkVector4& a_velocity) {
    return RE::NiPoint3(a_velocity.quad.m128_f32[0], a_velocity.quad.m128_f32[1], a_velocity.quad.m128_f32[2]);
}

void vibrateController(int hapticFrame, int length, bool isLeft) {
    auto papyrusVM = RE::BSScript::Internal::VirtualMachine::GetSingleton();
    if (!papyrusVM) return;

    RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;

    if (papyrusVM->TypeIsValid("VRIK"sv)) {
        int intensity = hapticFrame;
        int dur = length;
        auto args = RE::MakeFunctionArguments((bool)isLeft, (int)intensity, (int)dur);
        papyrusVM->DispatchStaticCall("VRIK"sv, "VrikHapticPulse"sv, args, callback);

    } else if (papyrusVM->TypeIsValid("Game"sv)) {
        float normStrength = (float)hapticFrame / 100.0f;
        if (normStrength > 1.0f) normStrength = 1.0f;
        float durationSec = (float)length / 1000000.0f;
        
        if (isLeft) {
             float leftInt = normStrength; float rightInt = 0.0f; float dur = durationSec;
             auto args = RE::MakeFunctionArguments((float)leftInt, (float)rightInt, (float)dur);
             papyrusVM->DispatchStaticCall("Game"sv, "ShakeController"sv, args, callback);
        } else {
             float leftInt = 0.0f; float rightInt = normStrength; float dur = durationSec;
             auto args = RE::MakeFunctionArguments((float)leftInt, (float)rightInt, (float)dur);
             papyrusVM->DispatchStaticCall("Game"sv, "ShakeController"sv, args, callback);
        }
    }
}

// WHITELIST HELPER
bool IsWhitelisted(RE::FormType t) {
    return t == RE::FormType::Static || 
           t == RE::FormType::MovableStatic ||
           t == RE::FormType::Tree ||
           t == RE::FormType::Flora ||
           t == RE::FormType::Furniture || 
           t == RE::FormType::Door ||
           t == RE::FormType::Activator ||
           t == RE::FormType::Container;
}

// Raycast Collision Check (v2.3 Target Layer 56 Fix)
ClimbHitData CheckClimbCollision(RE::Actor* player, bool isLeft, float rayDist) {
     ClimbHitData result;
     result.hit = false;
     
     auto playerCh = RE::PlayerCharacter::GetSingleton();
     if (!playerCh) return result;
     
     auto vrData = playerCh->GetVRNodeData();
     auto handNode = isLeft ? vrData->NPCLHnd : vrData->NPCRHnd;
     if (!handNode) return result;
     
     RE::NiMatrix3 rotation = handNode->world.rotate;
     RE::NiPoint3 forward = {rotation.entry[0][1], rotation.entry[1][1], rotation.entry[2][1]}; 
     RE::NiPoint3 right = {rotation.entry[0][0], rotation.entry[1][0], rotation.entry[2][0]}; 
     RE::NiPoint3 up = {rotation.entry[0][2], rotation.entry[1][2], rotation.entry[2][2]}; 
     RE::NiPoint3 dirDown = (forward - up); dirDown.Unitize();
     
     struct RayDir { RE::NiPoint3 dir; float maxDist; };
     RayDir rays[] = {
         { forward, rayDist },              
         { dirDown, rayDist * 0.8f },       
     };

     float startOffset = 2.0f; 
     RE::NiPoint3 baseStart = handNode->world.translate + (forward * startOffset);

     RE::bhkWorld* world = player->GetParentCell()->GetbhkWorld();
     if (!world) return result;
     
     const float havokScale = 0.0142875f;
     auto hkWorld = world->GetWorld1();
     if (!hkWorld) return result;

     for (const auto& ray : rays) {
         RE::NiPoint3 rStart = baseStart;
         RE::NiPoint3 rEnd = rStart + (ray.dir * ray.maxDist);
         
         RE::hkpWorldRayCastInput input;
         input.from = {rStart.x * havokScale, rStart.y * havokScale, rStart.z * havokScale, 0.0f}; 
         input.to = {rEnd.x * havokScale, rEnd.y * havokScale, rEnd.z * havokScale, 0.0f};
         
         RE::hkpWorldRayCastOutput output;
         hkWorld->CastRay(input, output);
         
         if (output.HasHit()) {
             const auto collidable = output.rootCollidable;
             if (collidable) {
                 auto& broadphase = collidable->broadPhaseHandle;
                 auto layer = broadphase.collisionFilterInfo & 0x7F; 
                 
                 // BLACKLIST
                 // 5=Weapon, 6=Projectile, 8=Biped, 32=CharController
                 // 56 = Custom Physics Layer detected in User Log (Pseudo Physics Weapon)
                 // Also blocking 57, 58 just in case.
                 if (layer == 5 || layer == 6 || layer == 8 || layer == 32 || layer >= 56) {
                     continue; 
                 }
                    
                 if (output.hitFraction < 0.01f) continue;
                 
                 result.hit = true;
                 result.normal.x = output.normal.quad.m128_f32[0];
                 result.normal.y = output.normal.quad.m128_f32[1];
                 result.normal.z = output.normal.quad.m128_f32[2];
                 
                 result.refr = RE::TESHavokUtilities::FindCollidableRef(*collidable);
                 
                 // STRICT WHITELIST
                 if (result.refr) {
                     if (result.refr->formID == 0x14) continue; 
                     
                     auto base = result.refr->GetBaseObject();
                     if (base) {
                         if (!IsWhitelisted(base->GetFormType())) {
                             result.hit = false; result.refr = nullptr; continue;
                         }
                     }
                 } else {
                     // BLOCK NULL REF if not Static/AnimStatic
                     if (layer != 1 && layer != 2 && layer != 3 && layer != 13) {
                         continue;
                     }
                 }
                 
                 return result; 
             }
         }
     }
     
     return result;
}

bool IsIce(RE::TESObjectREFR* ref) {
    if (!ref) return false;
    auto base = ref->GetBaseObject();
    if (!base) return false;
    const char* name = base->GetName();
    if (name) {
        std::string n = name;
        if (n.find("Ice") != std::string::npos || n.find("Glacier") != std::string::npos || n.find("Frozen") != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool IsClimbingTool(RE::Actor* player, bool isLeft) {
    if (!player) return false;
    auto obj = player->GetEquippedObject(isLeft);
    if (!obj) return false;
    if (obj->Is(RE::FormType::Weapon)) {
        auto weap = obj->As<RE::TESObjectWEAP>();
        if (!weap) return false;
        auto type = weap->GetWeaponType();
        using WType = RE::WEAPON_TYPE;
        if (type == WType::kOneHandAxe || type == WType::kOneHandDagger || type == WType::kOneHandMace) {
            return true;
        }
    }
    return false;
}
