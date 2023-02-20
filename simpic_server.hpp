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
#include <sys/sendfile.h>
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
#include "simpic_client.hpp"

#include "config.hpp"

namespace SimpicServerLib
{

    class SimpicServer
    {
    private:
        SimpicCache *cache;

        Logger new_moving_log;
        Logger new_activity_log;

        std::string recycle_bin;
        std::string alt_tmp;
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
