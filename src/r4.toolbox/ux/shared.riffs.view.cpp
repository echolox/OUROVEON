//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#include "pch.h"

#include "base/paging.h"
#include "base/eventbus.h"

#include "app/imgui.ext.h"
#include "app/module.frontend.fonts.h"
#include "colour/preset.h"

#include "endlesss/core.types.h"
#include "endlesss/toolkit.shares.h"

#include "mix/common.h"

#include "ux/user.selector.h"
#include "ux/shared.riffs.view.h"

using namespace endlesss;

namespace ux {

// ---------------------------------------------------------------------------------------------------------------------
struct SharedRiffView::State
{
    State( api::NetConfiguration::Shared& networkConfig, base::EventBusClient eventBus )
        : m_networkConfiguration( networkConfig )
        , m_eventBusClient( std::move( eventBus ) )
    {
        if ( m_networkConfiguration->hasAccess( api::NetConfiguration::Access::Authenticated ) )
        {
            m_user.setUsername( m_networkConfiguration->auth().user_id );
        }

        APP_EVENT_BIND_TO( MixerRiffChange );
        APP_EVENT_BIND_TO( NotifyJamNameCacheUpdated );
    }

    ~State()
    {
        APP_EVENT_UNBIND( NotifyJamNameCacheUpdated );
        APP_EVENT_UNBIND( MixerRiffChange );
    }

    void event_MixerRiffChange( const events::MixerRiffChange* eventData );
    void event_NotifyJamNameCacheUpdated( const events::NotifyJamNameCacheUpdated* eventData );


    void imgui(
        app::CoreGUI& coreGUI,
        endlesss::services::IJamNameCacheServices& jamNameCacheServices );


    void onNewDataFetched( toolkit::Shares::StatusOrData newData );
    void onNewDataAssigned();

    void restartJamNameCacheResolution()
    {
        m_jamNameCacheSyncIndex = 0;
        m_jamNameCacheUpdate = true;
    }


    api::NetConfiguration::Shared   m_networkConfiguration;
    base::EventBusClient            m_eventBusClient;

    std::atomic_bool                m_fetchInProgress = false;

    endlesss::types::RiffCouchID    m_currentlyPlayingRiffID;
    endlesss::types::RiffCouchIDSet m_enqueuedRiffIDs;

    base::EventListenerID           m_eventLID_MixerRiffChange = base::EventListenerID::invalid();
    base::EventListenerID           m_eventLID_NotifyJamNameCacheUpdated = base::EventListenerID::invalid();

    ImGui::ux::UserSelector         m_user;

    toolkit::Shares                 m_sharesCache;
    toolkit::Shares::StatusOrData   m_sharesData;
    
    std::string                     m_lastSyncTimestampString;

    std::vector< std::string >      m_jamNameResolvedArray;
    uint64_t                        m_jamNameCacheUpdateChangeIndex = std::numeric_limits<uint64_t>::max();
    std::size_t                     m_jamNameCacheSyncIndex = 0;
    bool                            m_jamNameCacheUpdate = false;

    bool                            m_currentlyPlayingSharedRiff = false;

    bool                            m_tryLoadFromCache = true;
    bool                            m_trySaveToCache = false;
};


// ---------------------------------------------------------------------------------------------------------------------
void SharedRiffView::State::event_MixerRiffChange( const events::MixerRiffChange* eventData )
{
    // might be a empty riff, only track actual riffs
    if ( eventData->m_riff != nullptr )
    {
        m_currentlyPlayingRiffID = eventData->m_riff->m_riffData.riff.couchID;
        m_enqueuedRiffIDs.erase( m_currentlyPlayingRiffID );
    }
    else
        m_currentlyPlayingRiffID = endlesss::types::RiffCouchID{};
}

// ---------------------------------------------------------------------------------------------------------------------
void SharedRiffView::State::event_NotifyJamNameCacheUpdated( const events::NotifyJamNameCacheUpdated* eventData )
{
    m_jamNameCacheUpdateChangeIndex = eventData->m_changeIndex;
    restartJamNameCacheResolution();
}

// ---------------------------------------------------------------------------------------------------------------------
void SharedRiffView::State::imgui(
    app::CoreGUI& coreGUI,
    endlesss::services::IJamNameCacheServices& jamNameCacheServices )
{
    // try to restore from the cache if requested; usually on the first time through, done here as we need CoreGUI / path provider
    if ( m_tryLoadFromCache )
    {
        auto newData = std::make_shared<config::endlesss::SharedRiffsCache>();

        const auto cacheLoadResult = config::load( coreGUI, *newData );

        // only complain if the file was malformed, it being missing is not an error
        if ( cacheLoadResult != config::LoadResult::Success &&
             cacheLoadResult != config::LoadResult::CannotFindConfigFile )
        {
            // not the end of the world but note it regardless
            blog::error::cfg( FMTX( "Unable to load shared riff cache [{}]" ), LoadResultToString( cacheLoadResult ) );
        }
        else
        {
            // stash if loaded ok
            m_sharesData = newData;
            m_user.setUsername( newData->m_username );

            onNewDataAssigned();
        }

        m_tryLoadFromCache = false;
    }

    // iteratively resolve jam names, one a frame
    // this process can be restarted if the jam name cache providers send out a message that new data has arrived
    if ( m_jamNameCacheUpdate && m_sharesData.ok() )
    {
        const auto dataPtr = *m_sharesData;

        // don't try and resolve non band##### IDs
        if ( dataPtr->m_personal[m_jamNameCacheSyncIndex] )
        {
            m_jamNameResolvedArray[m_jamNameCacheSyncIndex] = "[ personal ]";
        }
        else
        {
            // ask jame name services for data
            const bool bJamNameFound = jamNameCacheServices.lookupNameForJam(
                dataPtr->m_jamIDs[m_jamNameCacheSyncIndex],
                m_jamNameResolvedArray[m_jamNameCacheSyncIndex]
            );

            // if we get a cache miss, issue a fetch request to go plumb the servers for answers
            if ( !bJamNameFound )
            {
                coreGUI.getEventBusClient().Send< ::events::RequestJamNameRemoteFetch >(
                    dataPtr->m_jamIDs[m_jamNameCacheSyncIndex] );
            }
        }

        m_jamNameCacheSyncIndex++;
        if ( m_jamNameCacheSyncIndex >= m_jamNameResolvedArray.size() )
        {
            m_jamNameCacheUpdate = false;
        }
    }

    bool bFoundAPlayingRiffInTable = false;

    // management window
    if ( ImGui::Begin( ICON_FA_SHARE_FROM_SQUARE " Shared Riffs###shared_riffs" ) )
    {
        // check we have suitable network access to fetch fresh data
        const bool bCanSyncNewData = m_networkConfiguration->hasAccess( api::NetConfiguration::Access::Authenticated );
        const bool bIsFetchingData = m_fetchInProgress;

        // choose a user and fetch latest data on request
        {
            ImGui::Scoped::Enabled se( bCanSyncNewData && !bIsFetchingData );
            if ( ImGui::Button( " " ICON_FA_ARROWS_ROTATE " Fetch Latest " ) )
            {
                m_fetchInProgress = true;

                coreGUI.getTaskExecutor().run( std::move(
                    m_sharesCache.taskFetchLatest(
                        *m_networkConfiguration,
                        m_user.getUsername(),
                        [this]( toolkit::Shares::StatusOrData newData )
                        {
                            onNewDataFetched( newData );
                            m_fetchInProgress = false;
                        } ) 
                ));
            }
            ImGui::SameLine();

            ImGui::TextUnformatted( ICON_FA_USER_LARGE );
            ImGui::SameLine();
            m_user.imgui( coreGUI.getEndlesssPopulation(), 240.0f );
            ImGui::SameLine();
        }

        if ( bIsFetchingData )
        {
            ImGui::Spinner( "##syncing", true, ImGui::GetTextLineHeight() * 0.4f, 3.0f, 1.5f, ImGui::GetColorU32( ImGuiCol_Text ) );
            ImGui::SameLine();
            ImGui::TextUnformatted( " Working ..." );
        }
        else
        {
            if ( m_sharesData.ok() )
            {
                bool bScrollToPlaying = false;

                const auto dataPtr = *m_sharesData;

                ImGui::Text( "%i riffs, synced %s", dataPtr->m_count, m_lastSyncTimestampString.c_str() );
                ImGui::SameLine( 0, 0 );
                ImGui::RightAlignSameLine( 28.0f );
                {
                    ImGui::Scoped::Enabled scrollButtonAvailable( m_currentlyPlayingSharedRiff );
                    bScrollToPlaying = ImGui::IconButton( ICON_FA_ARROWS_DOWN_TO_LINE );
                }

                ImGui::Spacing();

                static ImVec2 buttonSizeMidTable( 29.0f, 21.0f );

                if ( ImGui::BeginChild( "##data_child" ) )
                {
                    ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, { 4.0f, 2.0f } );

                    if ( ImGui::BeginTable( "##shared_riff_table", 3,
                        ImGuiTableFlags_ScrollY |
                        ImGuiTableFlags_Borders |
                        ImGuiTableFlags_RowBg |
                        ImGuiTableFlags_NoSavedSettings ) )
                    {
                        ImGui::TableSetupScrollFreeze( 0, 1 );  // top row always visible

                        ImGui::TableSetupColumn( "Play", ImGuiTableColumnFlags_WidthFixed, 32.0f );
                        ImGui::TableSetupColumn( "Name", ImGuiTableColumnFlags_WidthStretch, 0.5f );
                        ImGui::TableSetupColumn( "Jam", ImGuiTableColumnFlags_WidthStretch, 0.5f );
                        ImGui::TableHeadersRow();

                        for ( std::size_t entry = 0; entry < dataPtr->m_count; entry++ )
                        {
                            const bool bIsPrivate       = dataPtr->m_private[entry];
                            const bool bIsPersonal      = dataPtr->m_personal[entry];
                            const bool bIsPlaying       = dataPtr->m_riffIDs[entry] == m_currentlyPlayingRiffID;
                            const bool bRiffWasEnqueued = m_enqueuedRiffIDs.contains( dataPtr->m_riffIDs[entry] );

                            // keep track of if any of the shared riffs are considered active
                            bFoundAPlayingRiffInTable |= bIsPlaying;

                            ImGui::PushID( (int32_t)entry );
                            ImGui::TableNextColumn();

                            // show some indication that work is in progress for this entry if it's been asked to play
                            if ( bRiffWasEnqueued )
                            {
                                ImGui::TableSetBgColor( ImGuiTableBgTarget_RowBg1, ImGui::GetPulseColour( 0.25f ) );
                            }

                            {
                                // riff enqueue-to-play button, disabled when in-flight
                                ImGui::Scoped::Disabled disabledButton( bRiffWasEnqueued );
                                ImGui::Scoped::ToggleButton highlightButton( bIsPlaying, true );
                                if ( ImGui::PrecisionButton( bRiffWasEnqueued ? ICON_FA_CIRCLE_CHEVRON_DOWN : ICON_FA_PLAY, buttonSizeMidTable, 1.0f ) )
                                {
                                    const endlesss::types::RiffCouchID sharedRiffToEnqueue( dataPtr->m_sharedRiffIDs[entry].c_str() );

                                    coreGUI.getEventBusClient().Send< ::events::EnqueueRiffPlayback >(
                                        endlesss::types::Constants::SharedRiffJam(),
                                        sharedRiffToEnqueue );

                                    // enqueue the riff ID, not the *shared* riff ID as the default riff ID is what will
                                    // be flowing back through "riff now being played" messages
                                    m_enqueuedRiffIDs.emplace( dataPtr->m_riffIDs[entry] );
                                }

                                if ( bIsPlaying && bScrollToPlaying )
                                    ImGui::ScrollToItem( ImGuiScrollFlags_KeepVisibleCenterY );
                            }
                            ImGui::TableNextColumn();

                            // draw the riff name
                            ImGui::Dummy( ImVec2( 0.0f, 1.0f ) );
                            if ( bIsPrivate )
                            {
                                ImGui::TextUnformatted( ICON_FA_LOCK );
                                ImGui::SameLine();
                            }
                            ImGui::TextUnformatted( dataPtr->m_names[entry] );
                            
                            ImGui::TableNextColumn();

                            // draw the jam name, if we have one
                            ImGui::Dummy( ImVec2( 0.0f, 1.0f ) );
                            {
                                const auto jamID = dataPtr->m_jamIDs[entry];

                                ImGui::TextDisabled( "ID" );
                                if ( ImGui::IsItemClicked() )
                                {
                                    ImGui::SetClipboardText( jamID.c_str() );
                                }
                                ImGui::CompactTooltip( jamID.c_str() );
                                ImGui::SameLine(0, 12.0f);

                                if ( bIsPrivate || bIsPersonal )
                                    ImGui::TextColored( colour::shades::callout.neutral(), m_jamNameResolvedArray[entry].c_str() );
                                else
                                    ImGui::TextUnformatted( m_jamNameResolvedArray[entry] );
                            }

                            ImGui::PopID();
                        }

                        ImGui::EndTable();
                    }

                    ImGui::PopStyleVar();
                }
                ImGui::EndChild();

                // if set in onNewDataFetched(), page any new data out to disk to cache the results across sessions
                if ( m_trySaveToCache )
                {
                    const auto cacheSaveResult = config::save( coreGUI, *dataPtr );
                    if ( cacheSaveResult != config::SaveResult::Success )
                    {
                        blog::error::cfg( FMTX( "Unable to save shared riff cache" ) );
                    }
                    m_trySaveToCache = false;
                }
            }
            else
            {
                ImGui::TextColored( ImGui::GetErrorTextColour(), ICON_FA_TRIANGLE_EXCLAMATION " Failed" );
                ImGui::CompactTooltip( m_sharesData.status().ToString().c_str() );
            }
        }
    }
    ImGui::End();

    m_currentlyPlayingSharedRiff = bFoundAPlayingRiffInTable;
}

// ---------------------------------------------------------------------------------------------------------------------
void SharedRiffView::State::onNewDataFetched( toolkit::Shares::StatusOrData newData )
{
    // snapshot the data, mark it to save on next cycle through the imgui call if it was successful
    m_sharesData = newData;

    onNewDataAssigned();

    if ( m_sharesData.ok() )
    {
        m_trySaveToCache = true;
    }
}

// ---------------------------------------------------------------------------------------------------------------------
void SharedRiffView::State::onNewDataAssigned()
{
    // clear out jam name resolution array, ready to refill
    m_jamNameResolvedArray.clear();

    if ( m_sharesData.ok() )
    {
        m_jamNameResolvedArray.resize( (*m_sharesData)->m_count );
        m_lastSyncTimestampString = spacetime::datestampStringFromUnix( (*m_sharesData)->m_lastSyncTime );
    }
    else
    {
        m_lastSyncTimestampString.clear();
    }

    restartJamNameCacheResolution();
}

// ---------------------------------------------------------------------------------------------------------------------
SharedRiffView::SharedRiffView( api::NetConfiguration::Shared& networkConfig, base::EventBusClient eventBus )
    : m_state( std::make_unique<State>( networkConfig, std::move( eventBus ) ) )
{
}

// ---------------------------------------------------------------------------------------------------------------------
SharedRiffView::~SharedRiffView()
{
}

// ---------------------------------------------------------------------------------------------------------------------
void SharedRiffView::imgui( app::CoreGUI& coreGUI, endlesss::services::IJamNameCacheServices& jamNameCacheServices )
{
    m_state->imgui( coreGUI, jamNameCacheServices );
}

} // namespace ux
