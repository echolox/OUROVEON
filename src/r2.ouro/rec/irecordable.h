//   _______ _______ ______ _______ ___ ___ _______ _______ _______ 
//  |       |   |   |   __ \       |   |   |    ___|       |    |  |
//  |   -   |   |   |      <   -   |   |   |    ___|   -   |       |
//  |_______|_______|___|__|_______|\_____/|_______|_______|__|____|
//  \\ harry denholm \\ ishani            ishani.org/shelf/ouroveon/
//
//  
//

#pragma once

namespace rec {

struct IRecordable
{
    virtual ~IRecordable() {}

    virtual bool beginRecording( const fs::path& outputPath, const std::string& filePrefix ) = 0;
    virtual void stopRecording() = 0;
    virtual bool isRecording() const = 0;
    virtual uint64_t getRecordingDataUsage() const = 0;
    virtual std::string_view getRecorderName() const = 0;
    virtual const char* getFluxState() const { return nullptr; }
};

} // namespace rec {
