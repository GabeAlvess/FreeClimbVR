#include <stddef.h>
#include "OnFrame.h"
#include "settings.h"
#include "Input.h"

using namespace SKSE;
using namespace SKSE::log;
using namespace SKSE::stl;

namespace {
    /**
     * Setup logging.
     */
    void InitializeLogging() {
        auto path = log_directory();
        if (!path) {
            report_and_fail("Unable to lookup SKSE logs directory.");
        }
        *path /= PluginDeclaration::GetSingleton()->GetName();
        *path += L".log";

        std::shared_ptr<spdlog::logger> log;
        if (IsDebuggerPresent()) {
            log = std::make_shared<spdlog::logger>("Global", std::make_shared<spdlog::sinks::msvc_sink_mt>());
        } else {
            log = std::make_shared<spdlog::logger>(
                "Global", std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true));
        }

        const auto level = spdlog::level::info;

        log->set_level(level);
        log->flush_on(level);

        spdlog::set_default_logger(std::move(log));
        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] [%t] [%s:%#] %v");
    }

    /**
     * Initialize the hooks.
     */
    void InitializeHooks() {
        log::info("About to hook frame update");
        ZacOnFrame::InstallFrameHook();
        log::trace("Hooks initialized.");
    }

    class HotReloadHandler : public RE::BSTEventSink<RE::MenuOpenCloseEvent> {
    public:
        static HotReloadHandler* GetSingleton() {
            static HotReloadHandler singleton;
            return &singleton;
        }

        RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override {
            // Reload on Console close or Journal Menu close (Pause menu)
            if (!a_event->opening && (a_event->menuName == "Console" || a_event->menuName == "Journal Menu")) {
                log::info("Menu closed. Reloading Settings from INI...");
                try {
                    Settings::GetSingleton()->Load();
                    log::info("Settings reloaded successfully.");
                } catch (...) {
                    log::error("Failed to reload settings.");
                }
            }
            return RE::BSEventNotifyControl::kContinue;
        }
    };

    void MessageHandler(SKSE::MessagingInterface::Message* a_msg) {
        switch (a_msg->type) {
            case SKSE::MessagingInterface::kDataLoaded: {
                log::info("kDataLoaded - Registering Input & Hot Reload"); 
                InputManager::GetSingleton()->Register(); // Register here!
                
                auto ui = RE::UI::GetSingleton();
                if (ui) {
                    ui->AddEventSink<RE::MenuOpenCloseEvent>(HotReloadHandler::GetSingleton());
                }
            } break;
            case SKSE::MessagingInterface::kPreLoadGame: {
                ZacOnFrame::CleanBeforeLoad();
            } break;
        }
    }
}  // namespace

/**
 * This is the main callback for initializing the SKSE plugin.
 */
SKSEPluginLoad(const LoadInterface* skse) {
    InitializeLogging();

    auto* plugin = PluginDeclaration::GetSingleton();
    auto version = plugin->GetVersion();
    log::info("{} {} is loading...", plugin->GetName(), version);

    Init(skse);

    try {
        Settings::GetSingleton()->Load();
    } catch (...) {
        logger::error("Exception caught when loading settings! Default settings will be used");
    }

    InitializeHooks();
    SKSE::GetMessagingInterface()->RegisterListener(MessageHandler);

    log::info("{} has finished loading.", plugin->GetName());
    return true;
}
