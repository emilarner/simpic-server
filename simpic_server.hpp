#pragma once

#include <iostream>
#include <fstream>
#include <vector>
#include <thread>
#include <map>
#include <set>
#include <unordered_set>
#include <mutex>

#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>

#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>

#include "sha256.hpp"
#include "phash/pHash.h"
#include "simpic_cache.hpp"
#include "simpic_protocol.hpp"
#include "networking.hpp"

#include "images.hpp"
#include "videos.hpp"
#include "audios.hpp"
#include "utils.hpp"

#include "config.hpp"

namespace SimpicServerLib
{
    class SimpicClient
    {
    public:
        SimpicCache *cache; 
        Logger *moving_log;
        Logger *main_log;

        std::string recycling_bin;

        struct sockaddr_in addr;
        int fd;

        

        /* Get the string representation of the client (its IPv4 address and its port. )*/
        std::string to_string();

        /* Given a path and a filename, move the file to the recycling bin, per the client's requests. */
        void deal_with_file(const std::string &path, const std::string &filename);

        /* Given a vector of Image pointers, send them to the client. */
        void set_of_pics(std::vector<Image*> *pics);

        /* Go through a directory, grab all of its files, and then send them to the client (simplified)*/
        int simpic_in_directory(std::string &dir, bool recursive, uint8_t max_ham);

        SimpicClient(SimpicCache *_cache, const std::string &recycle_bin, Logger *main, Logger *moving);
    };

    class SimpicServer
    {
    private:
        SimpicCache *cache;

        Logger new_moving_log;
        Logger new_activity_log;

        std::string recycle_bin;
        bool recycle_bin_on;

        int fd;
        struct sockaddr_in sock;
        uint16_t port;

        std::vector<SimpicClient*> clients;
        std::mutex cache_mutex;
        std::mutex client_mutex;

        std::set<std::pair<std::string, bool>> active_folders;
    public:
        std::function<void()> on_ready;

        SimpicServer(uint16_t _port, const std::string &simpic_dir, const std::string &_recycle_bin);
        SimpicServer(uint16_t _port);
        void start();
        void handler(SimpicClient *client);
        void save_cache();
    };
}
