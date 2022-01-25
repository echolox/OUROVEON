//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  ishani.org 2022              e.t.c.                  MIT License
//
//  
//

#pragma once

#include "config/base.h"

#include "base/utils.h"

namespace config {

struct Audio : public Base
{
    // data routing
    static constexpr auto StoragePath       = IPathProvider::PathFor::SharedConfig;
    static constexpr auto StorageFilename   = "audio.json";

    uint32_t        sampleRate = 44100;
    std::string     lastDevice = "";
    bool            lowLatency = true;


    template<class Archive>
    void serialize( Archive& archive )
    {
        archive( CEREAL_NVP( sampleRate )
               , CEREAL_NVP( lastDevice )
               , CEREAL_NVP( lowLatency )
        );
    }
};
using AudioOptional = std::optional< Audio >;

} // namespace config

