#pragma once

#include <iostream>
#include <fstream>
#include <map>
#include <unordered_set>
#include <vector>
#include <algorithm>
#include <mutex>
#include <thread>

#include <cstdio>
#include <errno.h>
#include <cstring>

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/un.h>

#include <openssl/sha.h>

#include "images.hpp"
#include "videos.hpp"
#include "audios.hpp"

#define SIMPIC_CACHE_MAGIC 0x00DEAD00


namespace SimpicServerLib
{
    /* File format */
    struct __attribute__((__packed__)) cache_header
    {
        uint32_t magic;
        uint32_t entries;
    };

    enum class CacheEntryTypes
    {
        Image,
        Video,
        Audio,
        Text,
        Undefined
    };

    typedef CacheEntryTypes SimpicEntryTypes;

    struct __attribute__((__packed__)) cache_entry
    {
        uint8_t type;
    };

    struct __attribute__((__packed__)) cache_image_entry
    {
        char sha256_hash[SHA256_DIGEST_LENGTH];
        uint64_t perceptual_hash;
        
        uint16_t width;
        uint16_t height;

        uint32_t size;
    };

    class SimpicCacheException : std::exception
    {
    public:
        int errnum;
        std::string msg;

        SimpicCacheException(std::string message, int _errno);
        std::string &what();
    };

    class SimpicMultipleInstanceException : std::exception
    {
    public:
        std::string msg;
        
        SimpicMultipleInstanceException(std::string message);
        std::string &what();
    };

    class SimpicCache
    {
    private:
        int lock_fd;

        std::string location;
        std::ofstream output;
        std::ifstream input;

        /* For images */
        std::map<sha256ptr_t, Image*, SHA256Comparator> cached;
        std::vector<std::pair<sha256ptr_t, Image*>> new_entries;

        /* For audio */

        std::map<sha256ptr_t, Audio*, SHA256Comparator> audio_cached;
        std::vector<std::pair<sha256ptr_t, Audio*>> new_audio_entries;

        /* For video */
        std::map<sha256ptr_t, Video*, SHA256Comparator> video_cached;
        std::vector<std::pair<sha256ptr_t, Video*>> new_video_entries;


    public:
        std::mutex saving_mutex;
        std::mutex entries_mutex;

        SimpicCache(std::string filename);
        ~SimpicCache();

        void init_cache_file();

        /* Can (and may) throw an exception. If it does not return 0, then an ERRNO was set.*/
        int readall();
        
        /* Also can throw an exception. */
        void saveall();

        void insert(Image *img);
        void insert(Video *vid);
        void insert(Audio *aud);

        std::map<sha256ptr_t, Image*, SHA256Comparator> *get_cached();
        Image *get_image(sha256ptr_t hash);

        /* Ascertain what kind of file it is from the extension. */
        static CacheEntryTypes get_type_from_extension(const std::string &ext);
    };
}
