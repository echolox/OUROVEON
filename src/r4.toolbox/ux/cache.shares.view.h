//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#pragma once

#include "app/core.h"
#include "base/utils.h"
#include "endlesss/core.types.h"


namespace ux {

struct SharedRiffView
{
    SharedRiffView( const std::string_view defaultUser );
    ~SharedRiffView();

    void imgui( app::CoreGUI& coreGUI );

private:

    struct State;
    std::unique_ptr< State >    m_state;

};

} // namespace ux
