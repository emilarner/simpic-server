#include "utils.hpp"

namespace SimpicServerLib
{
    Logger::Logger()
    {
        /* ISO 8601 Time Format */
        time_format = "%FT%TZ";
    }

    void Logger::set_time_format(const char *format)
    {
        time_format = format;
    }

    void Logger::write(const std::string &msg)
    {
        time_t now;
        std::time(&now);

        char result[32] = {0};
        std::strftime(result, sizeof(result), time_format, std::gmtime(&now));

        for (std::ofstream &fp : fps)
        {
            fp << "[" << result << "]: " << msg << std::endl;
            fp.flush();
        }
    }

    void Logger::open(const std::string &path)
    {
        fps.push_back(std::ofstream(path, std::ios::binary | std::ios::app));
    }

    Logger::~Logger()
    {
        for (std::ofstream &of : fps)
            of.close();
    }

    bool dir_is_child(const std::string &dir1, const std::string &dir2)
    {
        /* Tree Algebra has to be done here! */
        /* We need to check if dir1 intersect dir2 == dir1 || dir2. */
        /* where dir1 and dir2 represent their hierarchial tree nodes. */

        /* The parent directory cannot have a larger coordinate than the child, logically. */
        if (dir1.size() > dir2.size())
            return false;

        /* UNIX based systems. */
        std::vector<std::string> nodes1 = split_string(dir1, "/");
        std::vector<std::string> nodes2 = split_string(dir2, "/");


        /* Start taking away common elements, until one difference is observed. */
        std::vector<std::string> copy_nodes1 = nodes1;

        for (int i = 0; i < nodes1.size(); i++)
        {
            if (nodes1[i] != nodes2[i])
                break;

            copy_nodes1.erase(copy_nodes1.begin() + i);
        }

        /* If the entirety of the assumed parent node was taken away, */
        /* then it is true that the second node is its child, since their common node was found */
        /* to be the parent node. */

        /* If there are some elements left, this isn't the common node. */
        if (copy_nodes1.size() != 0)
            return false;
    

        return true;
    }

    std::vector<std::string> split_string(const std::string &str, const std::string &delimiter)
    {
        std::vector<std::string> result;

        char mut[str.size() + 1];
        std::strcpy(mut, str.c_str());

        char *token = std::strtok(mut, delimiter.c_str());

        while (token != nullptr)
        {
            result.push_back(std::string(token));
            token = std::strtok(nullptr, delimiter.c_str());
        }

        return result;
    }

    /* Get the home folder on UNIX systems. */
    std::string home_folder()
    {
        std::string result = "";

        /* If the effective or the actual user is root, consider them to be the root user calling upon this process, so use the root directory.*/

        if (!getuid() || !geteuid())
        {
            result += "/root/";
            return result;
        }

        char *username = getenv("USER");

        if (username == nullptr)
            return "";

        result += "/home/";
        result += username;
        result += "/";
        
        return result;
    }

    std::string simpic_folder(const std::string &home)
    {
        std::string result = "";
        result += home;
        result += ".simpic/";

        return result;
    }

    std::string concatenate_folder(std::string &one, std::string &two)
    {
        std::string result = "";

        result += one;
        result += "/";
        result += two;

        return result;
    }

    bool good_directory(std::string &where, bool print)
    {
        /* <filesystem> is not really fun... <dirent.h> is way better, albeit less crossplatform. */

        DIR *dir = opendir(where.c_str());

        if (dir == nullptr)
        {
            std::cerr << "Error with directory '" << where << "': " << std::strerror(errno) << std::endl;
            return false;
        }

        closedir(dir);
        return true;
    }

    void mkdir_dir(std::string &where)
    {
        if (good_directory(where, false))
            return;

        mkdir(where.c_str(), 0755);
    }

    std::string random_chars(uint8_t amount)
    {
        std::string result;
        char bank[] = "qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM";
        
        for (int i = 0; i < amount; i++)
            result += bank[rand() % sizeof(bank)];
        
        return result;
    }

    std::string get_extension(std::string filename)
    {
        const char *token = std::strchr(filename.c_str(), '.');
        const char *tmp = nullptr;

        if (token == nullptr)
            return "";

        while (true)
        {
            tmp = std::strchr(token + 1, '.');
            
            if (tmp == nullptr)
                return std::string(token + 1);

            token = tmp;
        }
    }
}