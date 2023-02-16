#include <iostream>
#include <vector>
#include <cstdio>
#include <dirent.h>
#include <algorithm>

#include <cstring>
#include <sys/stat.h>

#include "../images.hpp"
#include "../simpic_cache.hpp"

using namespace SimpicServerLib;

int main(int argc, char **argv, char **envp)
{
    SimpicCache cache("test-cache.simpic_cache");
    cache.readall();

    bool clear_cache = false;
    bool use_cache = true;

    for (int i = 1; i < argc; i++)
    {
        if (argv[i][0] != '-')
            continue;

        if (!std::strcmp(argv[i], "--clear-cache"))
        {
            clear_cache = true;
        }
        else if (!std::strcmp(argv[i], "--no-cache"))
        {
            use_cache = false;
        }
        else
        {
            std::cerr << "Unknown argument" << "\n";
            return -1; 
        }
    }

    std::string testing_directory = "dataset";
    std::vector<Image*> images;

    DIR *d = opendir(testing_directory.c_str());

    for (struct dirent *ent = readdir(d); ent != nullptr; ent = readdir(d))
    {
        if (ent->d_type != DT_REG)
            continue;
        
        std::string cpp_name(ent->d_name);
        std::string cpp_dir = testing_directory + (std::string)"/" + cpp_name;
        std::FILE *reading = std::fopen((testing_directory + "/" + cpp_name).c_str(), "rb");

        Image *img = nullptr;
        
        struct stat fileinfo;
        stat(cpp_dir.c_str(), &fileinfo);

        sha256_t image_hash[SHA256_DIGEST_LENGTH];
        SHA256CachedObject *image_hash_ptr;

        if ((image_hash_ptr = cache.get_sha256(cpp_dir, fileinfo.st_size, fileinfo.st_mtim.tv_sec)) == nullptr)
        {
            calculate_sha256(reading, image_hash);
            image_hash_ptr = new SHA256CachedObject(
                image_hash,
                std::time(nullptr),
                std::ftell(reading)
            );

            std::fseek(reading, 0, SEEK_SET);
            cache.insert({cpp_dir, image_hash_ptr});
        }

        if ((img = cache.get_image(image_hash_ptr->hash)) == nullptr || !use_cache)
        {
            img = new Image(testing_directory, cpp_name, reading, image_hash_ptr->hash);
            cache.insert(img);
            img->get_info(reading);
        }

        img->add_name(cpp_name);

        std::cout << cpp_name << "\n";
        std::cout << std::dec << img->height << "x" << std::dec << img->width << "\n";    
        
        std::cout << "\n" << "PHASH: " << std::uppercase << std::hex << img->phash << "\n" << "\n";

        images.push_back(img);
        std::fclose(reading);      
    }

    closedir(d);

    std::string count_text = "Current count of duplicates: ";
    std::vector<std::vector<Image*>*> groups = Image::find_similar_images(images, 4, [&count_text](int count) {
        std::cout << count_text << count << "\n"; 
    });

    for (std::vector<Image*>* group : groups)
    {
        std::cout << "SIMILAR IMAGES: " << "\n";
        
        for (Image *img : *group)
        {
            std::cout << "Name: " << img->filename << "\n";
        }

        std::cout << "\n";
    }

    cache.saveall();

    // memory leak
    return 0;
}