#include <RE/Skyrim.h>
#include "OnFrame.h"
#include "Sound.h"
#include <chrono>
#include <chrono>
#include "Input.h"

using namespace SKSE;
using namespace SKSE::log;

using namespace ZacOnFrame;

// Hook Installation
void ZacOnFrame::InstallFrameHook() {
    SKSE::AllocTrampoline(1 << 4);
    auto& trampoline = SKSE::GetTrampoline();

    REL::Relocation<std::uintptr_t> OnFrameBase{REL::RelocationID(35565, 36564)}; 
    _OnFrame =
        trampoline.write_call<5>(OnFrameBase.address() + REL::Relocate(0x748, 0xc26, 0x7ee), ZacOnFrame::OnFrameUpdate);

    REL::Relocation<std::uintptr_t> ProxyVTable{RE::VTABLE_bhkCharProxyController[1]};
    _SetVelocity = ProxyVTable.write_vfunc(0x07, ZacOnFrame::HookSetVelocity);
}

// Hook to override player velocity
void ZacOnFrame::HookSetVelocity(RE::bhkCharProxyController* controller, const RE::hkVector4& a_velocity) {
    if (!Settings::GetSingleton()->activeSettings.bEnableWholeMod) {
        _SetVelocity(controller, a_velocity);
        return;
    }

    auto& playerSt = PlayerState::GetSingleton();
    if (!playerSt.player || !playerSt.player->Is3DLoaded()) {
        _SetVelocity(controller, a_velocity);
        return;
    }

    // Usually jumpHeight is used to detect jump state in havok proxy
    // but specific struct members might vary. Assuming direct access works (SKSE dependent)
    // Actually, let's keep it simple.
    
    /* REMOVED: Allow climbing in water */

    // Priority: If Climbing (setVelocity is true), override everything immediately.
    // This allows catching ledges mid-jump without delay.
    if (playerSt.setVelocity) {
        // ... (rest is handled later, handled in original code?)
        // Wait, HookSetVelocity usually calls _SetVelocity OR suppresses it.
        // We need to see the rest of the function.
        // I will just fix the top part.
    }

    // (Water climbing logic removed)

    // Priority: If Climbing (setVelocity is true), override everything immediately.
    // This allows catching ledges mid-jump without delay.
    if (playerSt.setVelocity) {
        auto ourVelo = playerSt.velocity;
        _SetVelocity(controller, ourVelo);
        return;
    }

    if (auto charController = playerSt.player->GetCharController(); charController) {
        if (charController->flags.any(RE::CHARACTER_FLAGS::kJumping)) {
            playerSt.lastJumpFrame = iFrameCount;
            _SetVelocity(controller, a_velocity);
            return;
        }
    }
    
    // Jump Grace Period (Only applies if NOT climbing)
    int64_t conf_jumpExpireDur = 60; 
    if (iFrameCount - playerSt.lastJumpFrame < conf_jumpExpireDur && iFrameCount - playerSt.lastJumpFrame > 0) {
        _SetVelocity(controller, a_velocity);
        return;
    }

    bool isInMidAir = playerSt.player->IsInMidair();
    playerSt.isInMidAir = isInMidAir;
    if (!isInMidAir) playerSt.lastOngroundFrame = iFrameCount;

    // (Old location of setVelocity check was here, now moved up)
    
    _SetVelocity(controller, a_velocity);
}

// Global Variables for Loop
bool isOurFnRunning = false;
int64_t count_after_pause;



void ZacOnFrame::OnFrameUpdate() {
    if (Settings::GetSingleton()->activeSettings.bEnableWholeMod == false) {
        ZacOnFrame::_OnFrame();  
        return;
    }

    if (isOurFnRunning) {
        // log::warn("Our functions are running in parallel!!!"); 
    }
    isOurFnRunning = true;
    
    auto now = std::chrono::high_resolution_clock::now();
    bool isPaused = true;
    
    if (const auto ui{RE::UI::GetSingleton()}) {
        auto dur_last = std::chrono::duration_cast<std::chrono::microseconds>(now - last_time);
        last_time = now;
        
        // Pause detection (Loading screens, etc)
        if (dur_last.count() > 1000 * 1000) { 
            count_after_pause = 60;
            CleanBeforeLoad();
        }
        if (count_after_pause > 0) count_after_pause--;

        if (!ui->GameIsPaused() && count_after_pause <= 0) {
            
            // Race Detection / Settings Update (Every ~1 sec)
            if (iFrameCount % 60 == 0) {
                 if (auto player = RE::PlayerCharacter::GetSingleton()) {
                     if (auto race = player->GetRace()) {
                         const char* rid = race->GetFormEditorID();
                         if (rid) {
                             Settings::GetSingleton()->ApplyRace(rid);
                         }
                     }
                 }
            }

            // MAIN CLIMBING LOGIC
            // Calc dt in seconds
            float dt = (float)dur_last.count() / 1000000.0f;
            if (dt <= 0.0f) dt = 0.011f; // safety
            ClimbMain(dt);
            
        }
    }
    
    // Important: Call original OnFrame
    ZacOnFrame::_OnFrame();
    
    isOurFnRunning = false;
    iFrameCount++;
}

void ZacOnFrame::ClimbMain(float dt) {
    auto& playerSt = PlayerState::GetSingleton();
    auto player = playerSt.player;
    if (!player || !player->Is3DLoaded()) return;

    // Update basic states (Hand buffers for velocity calculation)
    playerSt.UpdateSpeedBuf();

    // Climbing Logic Variables
    RE::NiPoint3 totalClimbVelo(0.0f, 0.0f, 0.0f);
    bool isClimbing = false;
    int handsActive = 0;

    // Settings from INI (Using Active Settings which includes Race Overrides)
    auto& settings = Settings::GetSingleton()->activeSettings;
    float forceMulti = settings.fForceMulti;
    float rayDist = settings.fRayDist; 
    
    // Internal State
    static bool isHoldingL = false;
    static bool isHoldingR = false;
    static RE::NiPoint3 grabPointL; // Captured grab position
    static RE::NiPoint3 grabPointR; 
    static RE::NiPoint3 wallNormalL; // Captured wall normal
    static RE::NiPoint3 wallNormalR;

    static bool mustReleaseL = false;
    static bool mustReleaseR = false;
    
    // Smoothing State
    static bool wasClimbing = false;
    static float smoothingTimer = 0.0f;
    static RE::NiPoint3 entryVelo;
    static RE::NiPoint3 lastAppliedVelo; // To preserve momentum on release
    static float postReleaseTimer = 0.0f;   // Timer for sustained push-off
    static RE::NiPoint3 retainedWallNormal; // Wall normal at moment of release
    
    // Throw Window Tracker
    static RE::NiPoint3 peakThrowVelo(0,0,0);
    static float peakThrowTimer = 0.0f;
    
    // Haptic Cooldowns
    static int hapticCoolL = 0;
    static int hapticCoolR = 0;
    if (hapticCoolL > 0) hapticCoolL--;
    if (hapticCoolR > 0) hapticCoolR--;
    
    // Hover Haptic Skippers
    static int hoverSkip[2] = {0, 0};
    
    // Helper to update retained normal
    auto updateRetainedNormal = [&]() {
        RE::NiPoint3 n(0,0,0);
        int c = 0;
        if (isHoldingL) { n += wallNormalL; c++; }
        if (isHoldingR) { n += wallNormalR; c++; }
        if (c > 0) {
            n.x /= c; n.y /= c; n.z /= c;
            // Normalize
            float len = n.Length();
            if (len > 0.001f) {
                n.x /= len; n.y /= len; n.z /= len;
                retainedWallNormal = n;
            }
        }
    };

    // Check Hands
    auto checkHand = [&](bool isLeft) {
        bool& isHolding = isLeft ? isHoldingL : isHoldingR;
        RE::NiPoint3& grabPoint = isLeft ? grabPointL : grabPointR;
        RE::NiPoint3& wallNormal = isLeft ? wallNormalL : wallNormalR;
        bool& mustRelease = isLeft ? mustReleaseL : mustReleaseR;
        int& hapticCool = isLeft ? hapticCoolL : hapticCoolR;
        int hIdx = isLeft ? 0 : 1;

        // 1. Check Grip Input
        bool isGripping = false;
        if (auto inputMgr = InputManager::GetSingleton()) {
            isGripping = isLeft ? inputMgr->IsLeftGripPressed() : inputMgr->IsRightGripPressed();
        }
        
        // Handle Release
        if (!isGripping) {
            isHolding = false;
            mustRelease = false;
             // DO NOT RETURN! We must check for Hover.
        }

        if (mustRelease) return; // Wait for release

        // 2. Check Collision / Hold State
        auto playerCh = RE::PlayerCharacter::GetSingleton();
        if (!playerCh) return;
        auto vrData = playerCh->GetVRNodeData();
        auto handNode = isLeft ? vrData->NPCLHnd : vrData->NPCRHnd;
        if (!handNode) return;

        bool collision = false;
        
        // RAYCAST CHECK
        // We check every frame if not holding, to allow hover detection
        if (!isHolding) {
             auto hitData = CheckClimbCollision(player, isLeft, rayDist);
             if (hitData.hit) {
                 // --- HOVER LOGIC ---
                 if (!isGripping && settings.bEnableHaptics) {
                     // Subtle Pulse: Intensity 1 (Min), Duration 1ms, Interval 15 frames
                     // This simulates "Weak" vibration on VRIK/Oculus
                     if (hoverSkip[hIdx]++ > 15) {
                         vibrateController(1, 1000, isLeft);
                         hoverSkip[hIdx] = 0;
                     }
                 }
                 
                 // --- GRAB LOGIC ---
                 if (isGripping) {
                     // v1.3 Restoration: Safety & Ice Checks
                     if (hitData.refr) {
                         // 1. Self Grab Prevention
                         if (hitData.refr->formID == 0x14) {
                             // Do nothing, fail grab silently
                         } 
                         // 2. Ice Check
                         else if (IsIce(hitData.refr) && !IsClimbingTool(player, isLeft)) {
                             SKSE::log::info("Slipped on ICE! (Need Axe/Tools)");
                             // Play slip sound?
                             // Fail grab
                         }
                         else {
                             // Success
                             collision = true;
                             isHolding = true;
                             grabPoint = handNode->world.translate;
                             wallNormal = hitData.normal;
                             
                             // Play Material Sound
                             Sound::PlayClimbSound(hitData.refr, playerCh);
                             
                             const char* n = hitData.refr->GetName();
                             // SKSE::log::info("Grab Success: {}", n ? n : "Obj"); // Debug removed for cleanup

                             // Haptic Feedback (CLICK)
                             if (hapticCool <= 0 && settings.bEnableHaptics) {
                                 vibrateController(2, 40000, isLeft); // Impact click
                                 hapticCool = 30; 
                             }
                         }
                     } else {
                         // Static World Geometry -> Always grabbable
                         collision = true;
                         isHolding = true;
                         grabPoint = handNode->world.translate;
                         wallNormal = hitData.normal;
                         
                         // Play Default Sound (Stone/Static)
                         Sound::PlayClimbSound(nullptr, playerCh);
                         
                         if (hapticCool <= 0 && settings.bEnableHaptics) {
                             vibrateController(2, 40000, isLeft); 
                             hapticCool = 30; 
                         }
                     }
                 }
                 
             } else {
                 // No hit, reset hoverskip to avoid "stored" tick
                 hoverSkip[hIdx] = 10;
             }
        } else {
             // Already Holding - Sticky Logic
             collision = true; 
             
             // Check Arm Stretch
             RE::NiPoint3 currentHandPos = handNode->world.translate;
             float dist = currentHandPos.GetDistance(grabPoint);
             
             if (dist > settings.fMaxArmLength) {
                 isHolding = false;
                 mustRelease = true;
                 collision = false;
             }
        }

        // 3. Apply Climbing
        if (collision) {
             handsActive++;
             RE::NiPoint3 handVelo = playerSt.speedBuf.GetVelocity(3, isLeft);
             totalClimbVelo -= handVelo; 
             isClimbing = true;
        }
    };

    checkHand(true); 
    checkHand(false); 

    if (isClimbing) {
        updateRetainedNormal(); // Update normal continuously while holding
        postReleaseTimer = 0.0f; // Reset timer

        // Transition Check (Start of climb)
        if (!wasClimbing) {
             // Reset Peak Tracker
             entryVelo = {0,0,0};
             peakThrowVelo = {0,0,0};
             
             // Capture current failing velocity
             if (auto charCont = player->GetCharController()) {
                 RE::hkVector4 hkVelo;
                 charCont->GetLinearVelocityImpl(hkVelo);
                 entryVelo = Quad2Velo(hkVelo);
                 
                 // Smart Smoothing:
                 // Only smooth if we already had significant momentum (flying/falling).
                 // If we were stopped/grounded, DO NOT SMOOTH. Only immediate velocity can break ground friction.
                 if (entryVelo.Length() < 50.0f) {
                     smoothingTimer = 0.0f; // Instant grab from ground
                 } else {
                     smoothingTimer = settings.fGrabSmoothing;
                 }
             } else {
                 entryVelo = {0,0,0};
                 smoothingTimer = 0.0f;
             }
             
             // FIX: Cancel Jump Animation (Global - Once per climb)
             player->NotifyAnimationGraph("JumpLand");
         }

        // Stamina Logic
        float currentStamina = -1.0f;
        
        // Ensure we can access actor value
        if (auto avOwner = player->AsActorValueOwner()) {
             currentStamina = avOwner->GetActorValue(RE::ActorValue::kStamina);
        }
        
        if (settings.bEnableStamina && currentStamina >= 0.0f && currentStamina <= 1.0f) { // Consider < 1.0 as depleted to be safe
             // Force Release
             if (iFrameCount % 60 == 0) log::info("Stamina depleted! forcing release.");
             isHoldingL = false; isHoldingR = false;
             mustReleaseL = true; mustReleaseR = true;
             
        } else {
            // Apply Velocity
            totalClimbVelo = totalClimbVelo * forceMulti;
            
            /* REMOVED: Causing lateral drift (Pushing player sideways)
            // Constant Wall Push-Off (Safety Cushion)
            if (retainedWallNormal.Length() > 0.1f) {
                totalClimbVelo += (retainedWallNormal * 2.0f);
            }
            */

            // SMOOTHING BLEND
            if (smoothingTimer > 0.0f && settings.fGrabSmoothing > 0.0f) {
                float t = 1.0f - (smoothingTimer / settings.fGrabSmoothing); // 0.0 to 1.0
                // Clamp t (just in case)
                if (t < 0.0f) t = 0.0f; if (t > 1.0f) t = 1.0f;
                
                // Lerp: entryVelo -> totalClimbVelo
                RE::NiPoint3 diff = totalClimbVelo - entryVelo;
                totalClimbVelo = entryVelo + (diff * t);
                
                smoothingTimer -= dt;
            }

            // --- V2.4 MOTION SMOOTHING (Fix Jitter/Disorientation) ---
            // Filters out high-frequency noise from hand tracking and sudden spikes.
            // Alpha 0.4 provides a good balance between responsiveness and stability.
            // Static storage is simple for single-player.
            static RE::NiPoint3 s_lastFrameVelo = {0.0f, 0.0f, 0.0f};
            
            // Reset history on new climb
            if (!wasClimbing) {
                s_lastFrameVelo = totalClimbVelo;
            }
            
            float kAlpha = settings.fMotionSmoothing;
            if (kAlpha < 0.01f) kAlpha = 0.01f; // Avoid complete freeze
            if (kAlpha > 1.0f) kAlpha = 1.0f;
             
            totalClimbVelo = (totalClimbVelo * kAlpha) + (s_lastFrameVelo * (1.0f - kAlpha));
            s_lastFrameVelo = totalClimbVelo;
            // --------------------------------------------------------

            // Stamina Drain
            if (settings.bEnableStamina) {
                if (auto avOwner = player->AsActorValueOwner()) {
                    float velocityMagnitude = totalClimbVelo.Length();
                    
                    float costMove = settings.fStaminaCostMove;
                    float costIdle = settings.fStaminaCostIdle;
                    float moveThres = settings.fStaminaMovementThreshold;
    
                    float staminaCost = (velocityMagnitude > moveThres) ? costMove : costIdle;
                    
                    // One Hand Penalty
                    if (handsActive == 1) {
                        staminaCost *= settings.fStaminaOneHandCostMult;
                    }

                    avOwner->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kStamina, -staminaCost);
                }
            }

            // Fling / Throw Mechanics
            if (handsActive >= 2 && totalClimbVelo.z > 0.0f) {
                 float throwMult = settings.fThrowMult;
                 
                 totalClimbVelo.z *= throwMult; // Climbing boost
                 
                 float releaseThres = settings.fThrowReleaseThreshold;
                 if (totalClimbVelo.z > releaseThres) {
                     // Strong fling detected
                     isHoldingL = false; isHoldingR = false;
                     mustReleaseL = true; mustReleaseR = true;
                 }
            }

            // SAFETY: Clamp Maximum Velocity (Anti-Space Launch)
            float vLen = totalClimbVelo.Length();
            if (vLen > settings.fMaxVelocity) {
                totalClimbVelo = totalClimbVelo * (settings.fMaxVelocity / vLen);
            }
            
            // Wall Push-Off Logic was removed here to prevent player drift.
            
            lastAppliedVelo = totalClimbVelo; // Store for release

            // TRACK PEAK VELOCITY (For generous throw window)
            // If current upward velocity is higher than recorded peak, update it.
            if (totalClimbVelo.z > peakThrowVelo.z) {
                peakThrowVelo = totalClimbVelo;
                peakThrowTimer = settings.fThrowTimeWindow; // Configurable window
            }
            peakThrowTimer -= dt;
            if (peakThrowTimer <= 0.0f) {
                peakThrowVelo = totalClimbVelo; // Reset to current if timed out
            }

            playerSt.SetVelocity(totalClimbVelo.x, totalClimbVelo.y, totalClimbVelo.z);
            
            // Cancel Fall Damage & Animation logic (Only while actively holding)
            // ALWAYS Reset Fall Logic while holding (Essential for correct "Normal" physics)
            if (player->GetCharController()) {
                 player->GetCharController()->fallStartHeight = 0.0f;
                 player->GetCharController()->fallTime = 0.0f;
            }
            player->SetGraphVariableFloat("FallTime", 0.0f);
            
            playerSt.setVelocity = true;
        }
    } else {
        playerSt.setVelocity = false;
        
        // SAFETY: Post-Climb Immunity (If Enabled)
        if (postReleaseTimer > 0.0f && settings.bDisableFallDamage) {
             if (player->GetCharController()) {
                 player->GetCharController()->fallStartHeight = 0.0f;
                 player->GetCharController()->fallTime = 0.0f;
             }
        }
        
        // MOMENTUM INJECTION (The Fling Fix)
        if (wasClimbing) {
            postReleaseTimer = 2.0f; // Start Push-Off Timer
            
            // We just released the wall. Transfer momentum to game physics.
             if (auto charCont = player->GetCharController()) {
                 
                 // Which velocity to use? The instant one or the peak recent one?
                 // Use Peak if it offers better upward momentum (Fling)
                 RE::NiPoint3 launchVelo = lastAppliedVelo;
                 if (peakThrowVelo.z > launchVelo.z) {
                     launchVelo = peakThrowVelo;
                 }

                 // Clamp Vertical Fling
                 float finalZ = launchVelo.z;
                 if (finalZ > settings.fMaxFlingVelocity) finalZ = settings.fMaxFlingVelocity;
                 
                 RE::hkVector4 hkVelo;
                 hkVelo.quad = _mm_set_ps(0.0f, finalZ, launchVelo.y, launchVelo.x);
                 
                 // Apply slightly boosted momentum to help overcome air friction immediately
                 // Don't boost if it's just a gentle release. Threshold 200.0 is reasonable.
                 if (launchVelo.Length() > 200.0f) {
                      charCont->SetLinearVelocityImpl(hkVelo);
                 }
                 
                 // Reset trackers
                 peakThrowVelo = {0,0,0};
             }
        }
        
        // (Post-release push logic removed to prevent drift)
    }
    
    wasClimbing = isClimbing; // Update state for next frame
}

// Cleanup
void ZacOnFrame::CleanBeforeLoad() { 
    iFrameCount = 0;
    iLastPressGrip = 0;
    PlayerState::GetSingleton().Clear();
}

// Empty Stubs for any potential legacy links (though headers are clean now)
void ZacOnFrame::TimeSlowEffect(RE::Actor*, int64_t, float) {}
void ZacOnFrame::StopTimeSlowEffect(RE::Actor*) {}
RE::hkVector4 ZacOnFrame::CalculatePushVector(RE::NiPoint3, RE::NiPoint3, bool, float) { return RE::hkVector4(); }
