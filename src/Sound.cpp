#include "Sound.h"
#include <string>

namespace Sound {

    // Helper to find sound descriptor by Editor ID (e.g. "FSTRunStone")
    RE::BGSSoundDescriptorForm* GetLegacySound(const char* editorID) {
        auto form = RE::TESForm::LookupByEditorID(editorID);
        if (form) return form->As<RE::BGSSoundDescriptorForm>();
        return nullptr;
    }

    // Determine material type from reference
    // This is a heuristic approach since direct physics material query is complex via SKSE
    std::string PredictMaterial(RE::TESObjectREFR* ref) {
        if (!ref) return "Stone"; // Default to Stone (Terrain/Walls)

        auto base = ref->GetBaseObject();
        if (!base) return "Stone";

        // 1. Check Keywords
        // (Implementation omitted for brevity, requiring iteration over Keyword FormList)
        
        // 2. Check Name (Naive but effective for many objects)
        const char* nameC = base->GetName();
        if (nameC) {
            std::string name = nameC;
            // Lowercase for comparison could be better, but simple find works
            if (name.find("Wood") != std::string::npos || name.find("Tree") != std::string::npos || name.find("Log") != std::string::npos || name.find("Plank") != std::string::npos) return "Wood";
            if (name.find("Ice") != std::string::npos || name.find("Snow") != std::string::npos || name.find("Frozen") != std::string::npos) return "Snow";
            if (name.find("Metal") != std::string::npos || name.find("Iron") != std::string::npos || name.find("Steel") != std::string::npos || name.find("Dwarven") != std::string::npos) return "Metal";
            if (name.find("Dirt") != std::string::npos || name.find("Soil") != std::string::npos || name.find("Grass") != std::string::npos) return "Dirt";
        }

        // 3. Check Form Type
        auto type = base->GetFormType();
        if (type == RE::FormType::Tree) return "Wood";
        if (type == RE::FormType::Flora) return "Dirt";

        return "Stone";
    }

    void PlayClimbSound(RE::TESObjectREFR* surfaceRef, RE::Actor* player) {
        if (!player) return;

        // 1. Identify Material
        std::string mat = PredictMaterial(surfaceRef);

        // 2. Map to Sound Descriptor (Using Standard Footsteps as placeholders)
        // These are guaranteed to exist in Skyrim.esm
        const char* soundID = "FSTRunStone"; 
        
        if (mat == "Wood") soundID = "FSTRunWood";
        else if (mat == "Snow") soundID = "FSTRunSnow";
        else if (mat == "Metal") soundID = "FSTRunMetal"; // Or FSTArmorHeavyRun
        else if (mat == "Dirt") soundID = "FSTRunDirt";

        // 3. Play Sound
        auto soundDesc = GetLegacySound(soundID);
        if (soundDesc) {
            RE::BSSoundHandle handle;
            auto audioMgr = RE::BSAudioManager::GetSingleton();
            if (audioMgr && audioMgr->BuildSoundDataFromDescriptor(handle, soundDesc)) {
                // Set volume slightly lower for hands compared to feet
                handle.SetVolume(0.6f); 
                handle.SetPosition(player->GetPosition());
                handle.Play();
            }
        }
    }
}
