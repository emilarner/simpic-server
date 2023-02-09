#include "simpic_cache.hpp"

namespace SimpicServerLib
{
    SimpicCache::SimpicCache(std::string filename)
    {
        location = filename;
    }

    void SimpicCache::init_cache_file()
    {
        struct cache_header ch;
        ch.magic = SIMPIC_CACHE_MAGIC;
        ch.entries = 0;

        output = std::ofstream(location, std::ios::binary);
        output.write((char*) &ch, sizeof(ch));
        output.close();
    }

    int SimpicCache::readall()
    {
        std::FILE *fp = std::fopen(location.c_str(), "rb");

        if (fp == nullptr)
        {
            if (errno != ENOENT)
                return errno;

            return ENOENT;
        }

        struct cache_header ch;
        input = std::ifstream(location, std::ios::binary);
        input.read((char*) &ch, sizeof(ch));

        for (int i = 0; i < ch.entries; i++)
        {
            struct cache_entry main_ent;
            input.read((char*) &main_ent, sizeof(main_ent));

            switch ((CacheEntryTypes) main_ent.type)
            {
                case CacheEntryTypes::Image: 
                {
                    struct cache_image_entry ent;
                    input.read((char*) &ent, sizeof(ent));
                    ent.sha256_hash[sizeof(ent.sha256_hash)];

                    Image *img = new Image();

                    img->height = ent.height;
                    img->width = ent.width;
                    img->phash = ent.perceptual_hash;
                    std::memcpy(img->sha256, ent.sha256_hash, SHA256_DIGEST_LENGTH);
                    img->length = ent.size;

                    cached[img->sha256] = img;
                    break;
                }
            }
        }

        input.close();
        return 0;
    }

    void SimpicCache::saveall()
    {
        saving_mutex.lock();

        struct cache_header chdr;
        chdr.magic = SIMPIC_CACHE_MAGIC;
        chdr.entries = cached.size();

        std::ofstream writing(location, std::ios::binary | std::ios::app);

        /* Go to the beginning of the file and overwrite/write the header.*/
        writing.seekp(0, std::ios::beg);
        writing.write((char*) &chdr, sizeof(chdr));

        writing.seekp(0, std::ios::end);    

        /* Add each entry for *new* images. */
        /* A structured binding! */
        for (const auto &[key, value] : new_entries)
        {
            /* Writing the main cache_entry into the cache, detailing the type. */
            struct cache_entry main_entry;
            main_entry.type = (uint8_t) CacheEntryTypes::Image;
            writing.write((char*) &main_entry, sizeof(main_entry));

            struct cache_image_entry entry;

            std::memcpy(entry.sha256_hash, key, sizeof(entry.sha256_hash));

            entry.height = value->height;
            entry.width = value->width;
            entry.size = value->length;
            entry.perceptual_hash = value->phash;
            
            writing.write((char*) &entry, sizeof(entry));
        }

        new_entries.clear();
        writing.flush();
        writing.close();
        saving_mutex.unlock();
    }

    void SimpicCache::insert(Image *img)
    {
        entries_mutex.lock();

        /* If it is not already a part of the cache, make sure it is marked as a new entry. */
        if (cached.find(img->sha256) == cached.end())
            new_entries.push_back({img->sha256, img});

        cached[img->sha256] = img;
        entries_mutex.unlock();
    }

    Image *SimpicCache::get_image(sha256ptr_t hash)
    {
        std::map<sha256ptr_t, Image*, SHA256Comparator>::iterator it = cached.find(hash);

        if (it == cached.end())
            return nullptr;

        return (*(it)).second;
    }

    std::map<sha256ptr_t, Image*, SHA256Comparator> *SimpicCache::get_cached()
    {
        return &cached;
    }

    CacheEntryTypes SimpicCache::get_type_from_extension(const std::string &ext)
    {
        if (Image::type_from_extension(ext) != ImageType::Undefined)
            return CacheEntryTypes::Image;

    }
}