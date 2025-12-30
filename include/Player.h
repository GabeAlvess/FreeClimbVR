#include "Utils.h"
#include "Settings.h"

using namespace SKSE;


class SpeedRing {
public:
    const RE::NiPoint3 emptyPoint = RE::NiPoint3(123.0f, 0.0f, 0.0f);

    std::vector<RE::NiPoint3> bufferL;
    std::vector<RE::NiPoint3> bufferR;
    std::size_t capacity;  // how many latest frames are stored
    std::size_t indexCurrentL;
    std::size_t indexCurrentR;
    SpeedRing(std::size_t cap) : bufferL(cap), bufferR(cap), capacity(cap), indexCurrentR(0), indexCurrentL(0) {}

    void Clear() {
        for (std::size_t i = 0; i < capacity; i++) {
            bufferL[i] = emptyPoint;
            bufferR[i] = emptyPoint;
        }
    }

    void Push(RE::NiPoint3 p, bool isLeft) {
        if (isLeft) {
            bufferL[indexCurrentL] = p;
            indexCurrentL = (indexCurrentL + 1) % capacity;
        } else {
            bufferR[indexCurrentR] = p;
            indexCurrentR = (indexCurrentR + 1) % capacity;
        }
    }

    RE::NiPoint3 GetVelocity(std::size_t N, bool isLeft) const {
        if (N == 0 || N > capacity) {
            log::error("N is 0 or larger than capacity");
            return RE::NiPoint3(0.0f, 0.0f, 0.0f);
        }

        std::size_t currentIdx = isLeft ? indexCurrentL : indexCurrentR;
        const std::vector<RE::NiPoint3>& buffer = isLeft ? bufferL : bufferR;

        // Get the start and end positions
        RE::NiPoint3 startPos = buffer[(currentIdx - N + capacity) % capacity];
        RE::NiPoint3 endPos = buffer[(currentIdx - 1 + capacity) % capacity];

        auto diff1 = startPos - emptyPoint;
        auto diff2 = endPos - emptyPoint;
        if (diff1.Length() < 0.01f || diff2.Length() < 0.01f) {
            // log::error("startPos or endPos is empty"); 
            return RE::NiPoint3(0.0f, 0.0f, 0.0f);
        }

        // Calculate velocities
        RE::NiPoint3 velocityBottom = (endPos - startPos) / static_cast<float>(N);

        // Return the velocity
        return velocityBottom;
    }
};

class PlayerState {
public:
    RE::Actor* player;
    SpeedRing speedBuf;

    bool setVelocity;
    RE::hkVector4 velocity;
    
    // Climbing State
    float accumulatedStamCost = 0.0f;
    bool isInMidAir = false; 
    bool shouldCheckKnock = false; 
    
    int64_t lastOngroundFrame = 0;
    int64_t lastJumpFrame = 0;

    PlayerState()
        : player(nullptr),
          setVelocity(false),
          speedBuf(SpeedRing(100)) {}

    void Clear() { 
        setVelocity = false;
        velocity = RE::NiPoint3();
        isInMidAir = false;
        shouldCheckKnock = false;
        lastOngroundFrame = 0;
        lastJumpFrame = 0;
        speedBuf.Clear();
    }

    static PlayerState& GetSingleton() {
        static PlayerState singleton;
        if (singleton.player == nullptr) {
            // Get player
            auto playerCh = RE::PlayerCharacter::GetSingleton();
            if (!playerCh) {
                log::error("Can't get player!");
            }

            auto playerActor = static_cast<RE::Actor*>(playerCh);
            if (!playerActor) {
                log::error("Fail to cast player to Actor");
            }
            singleton.player = playerActor;
        }
        return singleton;
    }

    void SetVelocity(float x, float y, float z) {
        velocity = RE::hkVector4(x, y, z, 0.0f);
    }

    void CancelFallNumber() {
         if (player->GetCharController()) {
            player->GetCharController()->fallStartHeight = 0.0f;
            player->GetCharController()->fallTime = 0.0f;
        }
    }

    void UpdateSpeedBuf() {
        const auto actorRoot = netimmerse_cast<RE::BSFadeNode*>(player->Get3D());
        if (!actorRoot) {
            // log::warn("Fail to get actorRoot");
            return;
        }

        const auto nodeNameL = "NPC L Hand [LHnd]"sv;
        const auto nodeNameR = "NPC R Hand [RHnd]"sv;

        auto weaponNodeL = netimmerse_cast<RE::NiNode*>(actorRoot->GetObjectByName(nodeNameL));
        auto weaponNodeR = netimmerse_cast<RE::NiNode*>(actorRoot->GetObjectByName(nodeNameR));

        if (weaponNodeL && weaponNodeR) {
            auto playerPos = player->GetPosition();
            auto handPosL = weaponNodeL->world.translate - playerPos;
            auto handPosR = weaponNodeR->world.translate - playerPos;

            speedBuf.Push(handPosL, true);
            speedBuf.Push(handPosR, false);
        }
    }
};

