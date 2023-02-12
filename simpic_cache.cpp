#include "simpic_cache.hpp"

namespace SimpicServerLib
{
    SimpicCacheException::SimpicCacheException(std::string message, int _errno)
    {
        msg = message;
        errnum = _errno;
    }

    std::string &SimpicCacheException::what()
    {
        return msg;
    }

    SimpicMultipleInstanceException::SimpicMultipleInstanceException(std::string message)
    {
        msg = message;
    }

    std::string &SimpicMultipleInstanceException::what()
    {
        return msg;
    }

    SimpicCache::SimpicCache(std::string filename)
    {
        location = filename;

        /* The Simpic cache will get corrupted if multiple server instances are ran. */
        /* We use a UNIX socket to determine if an instance is running, to avoid */
        /* duplicates. */

        int connsockfd = socket(AF_UNIX, SOCK_STREAM, 0);
        struct sockaddr_un connsock;
        connsock.sun_family = AF_UNIX;
        std::strncpy(connsock.sun_path, "/tmp/simpic_server.locksock", sizeof(connsock.sun_path));

        /* The socket can be connected to, that means another instance is running!*/
        if (connect(connsockfd, (struct sockaddr*) &connsock, (socklen_t) sizeof(connsock)) != -1)
        {
            throw SimpicMultipleInstanceException(
                "Another instance of Simpic server is already running--that's not allowed."
            );
        }

        remove("/tmp/simpic_server.locksock");

        /* This thread runs the UNIX socket logic. */
        std::thread lock_unix([this]() -> void {
            int fd = socket(AF_UNIX, SOCK_STREAM, 0);
            this->lock_fd = fd;

            struct sockaddr_un sock;
            sock.sun_family = AF_UNIX;
            std::strncpy(sock.sun_path, "/tmp/simpic_server.locksock", sizeof(sock.sun_path));

            if (bind(fd, (struct sockaddr*) &sock, (socklen_t) sizeof(sock)) < 0)
            {
                throw SimpicCacheException(
                    "Failed to bind the simpic cache lock at /tmp/simpic_server.locksock",
                    errno
                );
            
            }

            listen(fd, 64);

            while (true)
            {
                int cfd = 0;
                struct sockaddr_un lol;
                socklen_t lol_size = sizeof(lol);

                if ((cfd = accept(fd, (struct sockaddr*) &lol, &lol_size) < 0))
                {
                    throw SimpicCacheException(
                        "Failed to accept a client for cache lock at /tmp/simpic_server.locksock",
                        errno
                    );
                }

                const char msg[] = "Open.";
                send(cfd, msg, sizeof(msg) + 1, 0);
                close(cfd);
            }       
        });

        lock_unix.detach();
    }

    SimpicCache::~SimpicCache()
    {
        close(lock_fd);
        unlink("/tmp/simpic_server.locksock");
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
        FILE *fp = std::fopen(location.c_str(), "rb");
        if (fp == nullptr)
            return 0;

        fclose(fp);
        
        struct cache_header ch;
        input = std::ifstream(location, std::ios::binary);
        input.read((char*) &ch, sizeof(ch));

        /* Erroneous magic. */
        if (ch.magic != SIMPIC_CACHE_MAGIC)
        {
            throw SimpicCacheException(
                (std::string)"The cache is corrupt and does not have the magic: " + location,
                0
            );
        }

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

        if (new_entries.size() == 0)
        {
            saving_mutex.unlock();
            return;
        }

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