#pragma once

#include <iostream>
#include <string>
#include <vector>

#include "sha256.hpp"
#include "simpic_cache.hpp"
#include "utils.hpp"
#include "phash/pHash.h"

namespace SimpicServerLib
{
    enum class VideoTypes
    {
        MP4
    };

    class Video
    {
    private:
        std::string filename;
        std::string path;

    };
}