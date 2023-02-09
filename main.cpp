#include <iostream>
#include <vector>

#include <cstdlib>
#include <cstring>
#include <ctime>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "simpic_server.hpp"
#include "utils.hpp"

using namespace SimpicServerLib;

void help()
{
    const char *msg =
    "simpic_server - The server daemon for Simpic.\n"
    "USAGE:\n\n"
    "-p, --port [PORT]              The port for Simpic to bind on. Default: 20202\n"
    "~~~~~~~^ you don't need to care about this, if not running as a service.\n"
    "-f, --force-delete             Don't move to a recycle bin, but completely delete.\n"
    "~~~~~~~^ not recommended.\n"
    "-r, --recycle-bin [PATH]       Set the recycle bin somewhere other than the default.\n"
    "-c, --cache [PATH]             Change the default directory of the cache.\n";

    std::cout << msg << std::endl;
}

int main(int argc, char **argv, char **envp)
{
    std::srand(std::time(nullptr));

    std::string home_dir = home_folder();
    std::string simpic_local_folder = simpic_folder(home_dir);
    std::string default_recycling_bin = simpic_local_folder + "recycling_bin/";
    std::string default_cache = simpic_local_folder + "cache.simpic_cache";

    /* Create the directory for simpic if it does not already exist. */
    mkdir_dir(simpic_local_folder);
    mkdir_dir(default_recycling_bin);

    char *recycling_bin = nullptr;
    bool force_delete = false;
    uint16_t port = 0;

    /* Go through each actual terminal argument. */
    for (int i = 1; i < argc; i++)
    {
        /* Not a command-line argument. */
        if (argv[i][0] != '-')
            continue;

        if (!std::strcmp(argv[i], "-r") || !std::strcmp(argv[i], "--recycle-bin"))
        {
            /* If the argument does not exist... we're going to be doing this a lot.*/
            if (argv[i + 1] == nullptr)
            {
                std::cerr << "-r/--recycle-bin requires an argument--namely, the directory for the recycling bin. We cannot continue, so we must exit." << "\n";

                return -2;
            }

            recycling_bin = argv[i + 1];
        }
        else if (!std::strcmp(argv[i], "-p") || !std::strcmp(argv[i], "--port"))
        {
            if (argv[i + 1] == nullptr)
            {
                std::cerr << "-p/--port requires an argument (the port)... exiting..." << "\n";
                return -3;
            }

            std::string strport(argv[i + 1]);

            uint32_t tmp_port = 0;

            /* If the port entered is not 100% alphanumeric, then std::stoi will throw an exception that needs to be handled.*/
            try
            {
                tmp_port = std::stoi(strport);
            }
            catch (std::exception &ex)
            {
                std::cerr << "Error parsing port '" << strport << "' in the arguments: " << ex.what() << "\n";

                return -4;
            }

            /* Checking if the uint32_t port is way too big for an actual port, before we cast (and overflow) this to a uint16_t. */
            if ((uint32_t)tmp_port > 65535)
            {
                std::cerr << "The port, " << tmp_port << ", is way too big for an unsigned 16 bit value (the ranges being between 0-65535 for valid ports). Exiting due to catastrophic error..." << "\n";

                return -5;
            }

            port = (uint16_t) tmp_port;
        }
        else if (!std::strcmp(argv[i], "-f") || !std::strcmp(argv[i], "--force-delete"))
            force_delete = true;

        else if (!std::strcmp(argv[i], "-h") || !std::strcmp(argv[i], "--help"))
        {
            help();
            return 0;
        }

        else
        {
            std::cerr << "Invalid argument '" << argv[i] << "'. Exiting..." << "\n";
            return -1;
        }
    }
    
    if (!port)
    {
        std::cerr << "Warning: Either you entered 0 as a port to bind on (which is invalid) or you did not specify a port to bind on. By default, we will be binding on PORT 20202. " << "\n";
        port = 20202;
    }

    /* C/C++ definitely does exist. We're having to constantly battle between C-style strings and the more contemporary std::string objects. */

    /* Moreover, there are entire sections of this program which are written in C, due to certain image and other media libraries being written in C. pHash especially exposes itself as a C library, rather than a C++ one. People like to say C/C++ doesn't exist to sound smarter or more knowledgable, but they're being extremely pedantic at this point. I think it is fair to say this program is written in C AND C++. */

    std::string cpp_recycling_bin = default_recycling_bin;

    /* Override default recycling bin. */
    if (recycling_bin != nullptr)
        cpp_recycling_bin = std::string(recycling_bin);

    /* If the recycling bin directory was invalid. */
    if (!good_directory(cpp_recycling_bin, true))
        return -6;


    /* Start the actual server after we've done all of the processing...*/
    SimpicServer sv(port, simpic_local_folder, cpp_recycling_bin);
    sv.start();
}