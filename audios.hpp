#pragma once

namespace SimpicServerLib
{
    enum class AudioTypes
    {
        MP3,
        WAV,
        FLAC
    };

    class Audio
    {
    private:
        std::string filename;
        std::string path;
        AudioTypes type;

    public:
        Audio(std::string _filename, std::string _path, AudioTypes _type);


    };
}