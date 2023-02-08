#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>

#include <unistd.h>
#include <dirent.h>
#include <cerrno>
#include <cstring>
#include <cstdlib>

#include <sys/stat.h>

#include <ctime>

namespace SimpicServerLib
{
    /* where dir1 is assumed parent and dir2 is assumed child, check if dir2 is actually a child. */
    bool dir_is_child(const std::string &dir1, const std::string &dir2);

    /* Split an std::string by a delimiter--uses std::strtok. */
    std::vector<std::string> split_string(const std::string &str, const std::string &delimiter);

    std::string home_folder();
    std::string simpic_folder(std::string &home);
    std::string concatenate_folder(std::string &one, std::string &two);

    bool good_directory(std::string &where, bool print);

    /* Create directory if it does not exist. */
    void mkdir_dir(std::string &where);

    std::string get_extension(std::string filename);
    std::string random_chars(uint8_t amount);
}