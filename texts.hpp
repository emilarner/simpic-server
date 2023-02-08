#pragma once

#include <iostream>
#include <string>

#include "phash/pHash.h"

namespace SimpicServerLib
{
    class Texts
    {
    private:
    public:
        static void compute_perceptual_hash(std::string &text);
        static void compute_perceptual_hash(const char *text);
        
    };
}