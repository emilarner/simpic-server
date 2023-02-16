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
        sha256_location = filename + (std::string)"_sha256";

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

        for (auto &[key, value] : cached)
            delete value;
        
        for (auto &[key, value] : sha256_cached)
            delete value;
    }

    int SimpicCache::readall()
    {
        std::FILE *fp = std::fopen(location.c_str(), "rb");
        std::FILE *fp2 = std::fopen(sha256_location.c_str(), "rb");

        if (fp2 != nullptr)
        {
            struct cache_sha256_header shahdr;

            std::ifstream sha256read(sha256_location, std::ios::binary);
            sha256read.read((char*) &shahdr, sizeof(shahdr));

            if (shahdr.magic != SIMPIC_SHA256_CACHE_MAGIC)
            {   
                sha256read.close();
                throw SimpicCacheException("The SHA256 cache magic isn't right; it is corrupt.", -1);
            }

            for (int i = 0; i < shahdr.entries; i++)
            {
                struct cache_sha256_entry shaent;
                sha256read.read((char*) &shaent, sizeof(shaent));

                SHA256CachedObject *shaobj = new SHA256CachedObject(
                    shaent.hash,
                    shaent.timestamp,
                    shaent.length
                );

                std::string result = "";

                int whole = shaent.path_len / 256;
                int frac = shaent.path_len % 256;

                char buffer[256];

                for (int j = 0; j < whole; j++)
                {
                    sha256read.read(buffer, 256);
                    result += buffer;
                }

                sha256read.read(buffer, frac);
                result += buffer;

                sha256_cached[result] = shaobj;
                result.clear();
            }

            sha256read.close();
            std::fclose(fp2);
        }

        if (fp != nullptr)
        {
            struct cache_header ch;
            input = std::ifstream(location, std::ios::binary);
            input.read((char*) &ch, sizeof(ch));

            /* Erroneous magic. */
            if (ch.magic != SIMPIC_CACHE_MAGIC)
            {
                input.close();

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
            std::fclose(fp);
        }
        return 0;
    }

    void SimpicCache::saveall()
    {
        saving_mutex.lock();

        /* Write updated header to SHA256 Cache file. */
        struct cache_sha256_header shahdr;
        shahdr.magic = SIMPIC_SHA256_CACHE_MAGIC;
        shahdr.entries = sha256_cached.size() + new_sha256_entries.size();
        std::ofstream sha256_write(sha256_location, std::ios::binary | std::ios::app);
        sha256_write.write((const char*)&shahdr, sizeof(shahdr));
        sha256_write.seekp(0, std::ios::end);

        for (std::pair<std::string, SHA256CachedObject*> item : new_sha256_entries)
        {
            struct cache_sha256_entry shaent;

            // inefficient but organized
            std::memcpy(shaent.hash, item.second->hash, SHA256_DIGEST_LENGTH);
            shaent.length = item.second->length;
            shaent.timestamp = item.second->timestamp;
            shaent.path_len = item.first.size() + 1;

            sha256_write.write((const char*) &shaent, sizeof(shaent));
            sha256_write.write((const char*) item.first.c_str(), shaent.path_len);
        }

        new_sha256_entries.clear();
        sha256_write.close();

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
        //entries_mutex.lock();

        /* DATA RACE!!! If someone updates the new_entries list at the same time
           another thread saves the cache, then information will be lost! 
           thus, after all, we must use the same mutex when inserting and saving. */

        saving_mutex.lock();

        /* If it is not already a part of the cache, make sure it is marked as a new entry. */
        if (cached.find(img->sha256) == cached.end())
            new_entries.push_back({img->sha256, img});

        cached[img->sha256] = img;

        saving_mutex.unlock();
        //entries_mutex.unlock();
    }

    void SimpicCache::insert(std::pair<std::string, SHA256CachedObject*> shaobj)
    {
        saving_mutex.lock();

        new_sha256_entries.push_back(shaobj);
        sha256_cached[shaobj.first] = shaobj.second;

        saving_mutex.unlock();
    }

    Image *SimpicCache::get_image(sha256ptr_t hash)
    {
        std::map<sha256ptr_t, Image*, SHA256Comparator>::iterator it = cached.find(hash);

        if (it == cached.end())
            return nullptr;

        return it->second;
    }

    SHA256CachedObject *SimpicCache::get_sha256(const std::string &path, uint64_t length, uint64_t timestamp)
    {
        std::unordered_map<std::string, SHA256CachedObject*>::iterator 
                it = sha256_cached.find(path);

        if (it == sha256_cached.end())
            return nullptr;

        SHA256CachedObject *obj = it->second;

        /* If these are different, we can be 99% sure the hash is different. */
        /* Due to the data structure of the cache file, removing items is very expensive. */
        /* Instead, we can just let the newest occurance of the path in the cache file */
        /* overwrite all other instances once it is put into the std::unordered_ map<K, V>. */
        /* This is fine, because this is a rather rare occurance. */

        if (obj->length != length)
            return nullptr;

        if (obj->timestamp != timestamp)
            return nullptr;

        
        /* Then why not just associate the perceptual hash with the path then??? */
        /* This builds a more robust catalogue of images, because paths often and will */
        /* change throughout the duration of a filesystem's existence. */
        /* This will greatly increase the performance of this program with files > 100MiB*/
        /* while using std::string as the key instead would not really do that much... */

        return obj;
    }

    CacheEntryTypes SimpicCache::get_type_from_extension(const std::string &ext)
    {
        if (Image::type_from_extension(ext) != ImageType::Undefined)
            return CacheEntryTypes::Image;

        return CacheEntryTypes::Undefined;
    }
}