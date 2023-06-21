//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  base class for general app framework
//

#pragma once

#include "base/eventbus.h"

#include "spacetime/moment.h"

#include "config/data.h"
#include "config/performance.h"
#include "config/frontend.h"

#include "endlesss/config.h"
#include "endlesss/api.h"
#include "endlesss/cache.jams.h"
#include "endlesss/cache.shares.h"
#include "endlesss/cache.stems.h"
#include "endlesss/toolkit.exchange.h"
#include "endlesss/toolkit.riff.export.h"

// ---------------------------------------------------------------------------------------------------------------------
// on Windows, declare and enable use of the global memory IPC object for pushing Exchange data out to external apps
#if OURO_PLATFORM_WIN

#include "win32/ipc.h"
#define OURO_EXCHANGE_IPC   1

namespace endlesss {
using ExchangeIPC = win32::GlobalSharedMemory< endlesss::toolkit::Exchange >;
} //namespace endlesss

#else // OURO_PLATFORM_WIN

#define OURO_EXCHANGE_IPC   0

#endif

template <typename T>
concept ReturnsAbslStatus = requires (T t) {
    { t() } -> std::same_as<absl::Status>;
};

template< ReturnsAbslStatus _ccall >
bool checkedCoreCall( const std::string_view& context, const _ccall& cb )
{
    if ( const auto callStatus = cb(); !callStatus.ok() )
    {
        blog::error::core( FMTX( "{} failed; {}" ),
            context,
            callStatus.ToString() );

        return false;
    }
    return true;
}


// ---------------------------------------------------------------------------------------------------------------------
namespace app {

// #HDD todo, move this somewhere, add accessors etc
struct StoragePaths
{
    StoragePaths() = delete;
    StoragePaths( const config::Data& configData, const char* appName );

    fs::path     cacheCommon;    // config::data::storageRoot / cache / common name
    fs::path     cacheApp;       // config::data::storageRoot / cache / app name

    fs::path     outputApp;      // config::data::storageRoot / output / app name

    // utility to try and ensure the populated paths exist, (try to) create them if they don't;
    // returns false if this process fails, logs out all the attempts
    bool tryToCreateAndValidate() const;
};


namespace module { struct Audio; struct Midi; }
using AudioModule = std::unique_ptr<module::Audio>;
using MidiModule  = std::unique_ptr<module::Midi>;

// ---------------------------------------------------------------------------------------------------------------------

// initial thread startup wrapper
struct CoreStart
{
    CoreStart();
    virtual ~CoreStart();
};

// some access to Core app instances without having to hand over everything
struct ICoreServices
{
    virtual ~ICoreServices() {}

    virtual app::AudioModule&                   getAudioModule() = 0;
    virtual app::MidiModule&                    getMidiModule() = 0;
    virtual const endlesss::toolkit::Exchange&  getEndlesssExchange() = 0;
    virtual tf::Executor&                       getTaskExecutor() = 0;
    virtual sol::state_view&                    getLuaState() = 0;
};

// exposure of custom render injection callbacks as interface
struct ICoreCustomRendering
{
    using RenderInjectionCallback = std::function< void() >;

    enum class RenderPoint
    {
        PreImgui,       // blit things to the screen before the imgui render launches; only useful if not drawing fullscreen/docked imgui stuff that will overwrite it immediately
        PostImgui       // blit things on top of the drawn UI
    };

    // render callbacks are only valid for a single frame, client code should re-add them constantly if required
    virtual void registerRenderCallback( const RenderPoint rp, const RenderInjectionCallback& callback ) = 0;
};


// ---------------------------------------------------------------------------------------------------------------------
// application base class that implements basic services for headless use
//
struct Core : CoreStart,
              config::IPathProvider,
              ICoreServices
{
    DECLARE_NO_COPY( Core );

    Core();
    ~Core();

    // high-level entrypoint, called by main()
    int Run();

    // used for visual identity as well as cache path differentiation
    virtual const char* GetAppName() const = 0;             // 'FOO'
    virtual const char* GetAppNameWithVersion() const = 0;  // 'FOO 0.1.2-beta'
    virtual const char* GetAppCacheName() const = 0;        // 'foo' - must be filename/path friendly

    // optional toggle for supporting login-less boot up where the app can run without needing full auth data
    virtual bool supportsOfflineEndlesssMode() const { return false; }

    ouro_nodiscard constexpr const fs::path& getSharedConfigPath() const { return m_sharedConfigPath; }
    ouro_nodiscard constexpr const fs::path& getSharedDataPath() const   { return m_sharedDataPath;   }
    ouro_nodiscard constexpr const fs::path& getAppConfigPath() const    { return m_appConfigPath;    }


    static void waitForConsoleKey();

protected:

    // called once basic initial configuration is done for the application to continue work
    virtual int Entrypoint() = 0;

    ouro_nodiscard constexpr const char* getOuroveonPlatform() const 
    {
#if OURO_PLATFORM_WIN
        return "Windows";
#elif OURO_PLATFORM_OSX
        return "MacOS";
#elif OURO_PLATFORM_NIX
        return "Linux";
#else
        return "Unknown";
#endif
    }


    // call to transmit or broadcast the currently populated endlesss::Exchange block if such methods are enabled
    // followed by clearing it ready for re-populating
    void emitAndClearExchangeData();


    fs::path                                m_sharedConfigPath;             // R/W path for config shared between apps
    fs::path                                m_sharedDataPath;               // RO  path for app shared data in the install folder
    fs::path                                m_appConfigPath;                // R/W path for config for this specific app

    config::DataOptional                    m_configData = std::nullopt;
    config::Performance                     m_configPerf;
    config::endlesss::rAPI                  m_configEndlesssAPI;            // loaded from app install, will be used with
                                                                            // an auth block to initialise NetConfiguration

    // mash of Endlesss access configuration and authentication credentials, required for all our API calls
    // <optional> as this may require a login process during boot sequence and user may choose to work offline
    std::optional< endlesss::api::NetConfiguration >
                                            m_apiNetworkConfiguration = std::nullopt;

    // multithreading bro ever heard of it
    tf::Executor                            m_taskExecutor;

    // application-wide lua state wrapper
    sol::state                              m_lua;


    // master event bus
    base::EventBusPtr                       m_appEventBus;
    std::optional< base::EventBusClient >   m_appEventBusClient = std::nullopt;


    // the cached jam metadata - public names et al
    endlesss::cache::Jams                   m_jamLibrary;


    // standard state exchange data, filled when possible with the current playback state
    endlesss::toolkit::Exchange             m_endlesssExchange;

#if OURO_EXCHANGE_IPC
    // constantly-updated globally-shared data block
    endlesss::ExchangeIPC                   m_endlesssExchangeIPC;
    uint32_t                                m_endlesssExchangeWriteCounter = 1;
#endif

    // the interface to portaudio, make noise go bang
    app::AudioModule                        m_mdAudio;

    // interface to MIDI capture/production
    app::MidiModule                         m_mdMidi;

public:

    inline const base::EventBusClient& getEventBusClient() const
    { 
        ABSL_ASSERT( m_appEventBusClient.has_value() );
        return m_appEventBusClient.value();
    }

    // config::IPathProvider
    ouro_nodiscard xconstexpr fs::path getPath( const PathFor p ) const override
    { 
        switch ( p )
        {
            case config::IPathProvider::PathFor::SharedConfig:  return m_sharedConfigPath;
            case config::IPathProvider::PathFor::SharedData:    return m_sharedDataPath;
            case config::IPathProvider::PathFor::PerAppConfig:  return m_appConfigPath;
        }
        ABSL_ASSERT( 0 );
        return { "" };
    }

    // ICoreServices
    app::AudioModule&                   getAudioModule() override       { return m_mdAudio; }
    app::MidiModule&                    getMidiModule() override        { return m_mdMidi; }
    const endlesss::toolkit::Exchange&  getEndlesssExchange() override  { return m_endlesssExchange; }
    tf::Executor&                       getTaskExecutor() override      { return m_taskExecutor; }
    sol::state_view&                    getLuaState() override          { return m_lua; }
};


// ---------------------------------------------------------------------------------------------------------------------
namespace module { struct Frontend; }
using FrontendModule = std::unique_ptr<module::Frontend>;

// ---------------------------------------------------------------------------------------------------------------------
// next layer up from Core is a Core app with a UI; bringing GL and ImGUI into the mix
//
struct CoreGUI : Core,
                 ICoreCustomRendering
{
    // UI injection for inserting custom things into generic structure - eg. main menu items, status bar blocks
    using UIInjectionHandle         = uint32_t;
    using UIInjectionHandleOptional = std::optional< UIInjectionHandle >;
    using UIInjectionCallback       = std::function< void() >;

    using ModalPopupExecutor        = std::function< void( const char* ) >;
    
    using FileDialogInst            = std::unique_ptr<ImGuiFileDialog>;
    using FileDialogCallback        = std::function< void( ImGuiFileDialog& ) >;


    enum ViewportFlags
    {
        VF_None             = 0,
        VF_WithDocking      = 1 << 1,
        VF_WithMainMenu     = 1 << 2,
        VF_WithStatusBar    = 1 << 3,
    };

    using MainLoopCallback = std::function<void()>;

    // ImGui panel displaying gathered performance metrics in a table
    void ImGuiPerformanceTracker();


    enum class StatusBarAlignment
    {
        Left,
        Right
    };

    UIInjectionHandle registerStatusBarBlock( const StatusBarAlignment alignment, const float size, const UIInjectionCallback& callback );
    bool unregisterStatusBarBlock( const UIInjectionHandle handle );

    UIInjectionHandle registerMainMenuEntry( const int32_t ordering, const std::string& menuName, const UIInjectionCallback& callback );
    bool unregisterMainMenuEntry( const UIInjectionHandle handle );


    // ICoreCustomRendering
    void registerRenderCallback( const RenderPoint rp, const RenderInjectionCallback& callback ) override;


    // push a popup label to execute in the edges of the main loop, with [executor] being called to actually display
    // whatever popup window you have in mind.
    void activateModalPopup( const char* label, const ModalPopupExecutor& executor );

    // submit an ImGui file dialog instance for display as part of the main loop; we can only have one live at once
    inline bool activateFileDialog( FileDialogInst&& dialogInstance, const FileDialogCallback& onOK )
    {
        if ( m_activeFileDialog == nullptr )
        {
            m_activeFileDialog       = std::move(dialogInstance);
            m_fileDialogCallbackOnOK = onOK;
            return true;
        }
        return false;
    }

    ouro_nodiscard constexpr const app::FrontendModule& getFrontend() const { return m_mdFrontEnd; }


protected:

    struct StatusBarBlock
    {
        StatusBarBlock( const UIInjectionHandle& handle,
                        const float size,
                        const UIInjectionCallback& callback )
            : m_handle( handle )
            , m_size( size )
            , m_callback( callback )
        {}

        UIInjectionHandle       m_handle;
        float                   m_size;
        UIInjectionCallback     m_callback;
    };
    using StatusBarBlockList = std::vector< StatusBarBlock >;

    struct MenuMenuEntry
    {
        int32_t                             m_ordering = 0;
        std::string                         m_name;
        std::vector< UIInjectionHandle >    m_handles;
        std::vector< UIInjectionCallback >  m_callbacks;
    };
    using MenuMenuEntryList     = std::vector< MenuMenuEntry >;

    using CustomRenderCallbacks = std::vector< ICoreCustomRendering::RenderInjectionCallback >;


    // generic modal-popup tracking types to simplify client code opening and running dialog boxes
    using ModalPopupsWaiting    = std::vector< std::string >;
    using ModalPopupsActive     = std::vector< std::tuple< std::string, ModalPopupExecutor > >;


    // to avoid flickering values on the status bar; distracting and not particularly useful
    using AudioLoadAverage      = base::RollingAverage< 60 >;


    // app can declare its own frontend configuration blob
    virtual config::Frontend createDefaultFrontendConfig() const;


    // from Core
    // implementation of Core entrypoint to adorn with GUI components, then calling the expanded one below
    virtual int Entrypoint() override;

    // once services all started, this will be called to begin app-specific main loop; return exit value 
    virtual int EntrypointGUI() = 0;


    // call inside app main loop to perform pre/post core functions (eg. checking for exit, submitting rendering)
    bool beginInterfaceLayout( const ViewportFlags viewportFlags );
    void finishInterfaceLayoutAndRender();

    static ouro_nodiscard constexpr bool hasViewportFlag( const ViewportFlags viewportFlags, ViewportFlags vFlag )
    {
        return ( viewportFlags & vFlag ) == vFlag;
    }

    // register a developer menu entry by name, will toggle the bool pointer on menu selection
    inline void addDeveloperMenuFlag( std::string menuName, bool* boolFlagAddress )
    {
        ABSL_ASSERT( boolFlagAddress != nullptr );
        *boolFlagAddress = false;
        m_developerMenuRegistry.emplace( std::move( menuName ), boolFlagAddress );
    }


    config::Frontend        m_configFrontend;
    app::FrontendModule     m_mdFrontEnd;       // UI canvas management


    // collection of performance data for core systems
    struct PerfData
    {
        spacetime::Moment           m_moment;       // tracking timer

        std::chrono::milliseconds   m_uiEventBus;
        std::chrono::milliseconds   m_uiPreRender;
        std::chrono::milliseconds   m_uiPostRender;
    }                       m_perfData;
    AudioLoadAverage        m_audoLoadAverage;


    UIInjectionHandle       m_injectionHandleCounter = 0;

    // registered new core items like menu entries, status bar sections
    MenuMenuEntryList       m_mainMenuEntries;
    StatusBarBlockList      m_statusBarBlocksLeft;
    StatusBarBlockList      m_statusBarBlocksRight;

    // modal dialog registration to unify the handling of arbitrary popups during input loop
    ModalPopupsWaiting      m_modalsWaiting;
    ModalPopupsActive       m_modalsActive;

    // rendering phase injections, only valid for one frame; clients should re-up the registrations each time
    CustomRenderCallbacks   m_preImguiRenderCallbacks;
    CustomRenderCallbacks   m_postImguiRenderCallbacks;


    // instance of the file picker imgui gizmo
    FileDialogInst          m_activeFileDialog;
    FileDialogCallback      m_fileDialogCallbackOnOK;

private:

    using DeveloperFlagRegistry = absl::flat_hash_map< std::string, bool* >;


    void checkLayoutConfig();

    DeveloperFlagRegistry   m_developerMenuRegistry;

#if OURO_DEBUG
    bool                    m_showImGuiDebugWindow      = false;
#endif // OURO_DEBUG
    bool                    m_showPerformanceWindow     = false;
    bool                    m_showCommandPaletteWindow  = false;
    bool                    m_resetLayoutInNextUpdate   = false;
};

} // namespace app

