#include "sha256.hpp"

namespace SimpicServerLib
{
    bool SHA256Comparator::operator()(const char *sha1, const char *sha2) const
    {
        return std::memcmp(sha1, sha2, SHA256_DIGEST_LENGTH) > 0;
    }

    sha256ptr_t calculate_sha256(std::FILE *fp, sha256ptr_t where)
    {
        SHA256_CTX ctx;
        SHA256_Init(&ctx);

        size_t amnt;
        char buffer[BUFFER_SIZE];

        while (!std::feof(fp))
        {
            amnt = std::fread(buffer, 1, sizeof(buffer), fp);
            SHA256_Update(&ctx, buffer, amnt);
        } 

        SHA256_Final((unsigned char*) where, &ctx);
        return where;
    }

    SHA256CachedObject::SHA256CachedObject(sha256ptr_t _hash, int64_t _timestamp, uint64_t _length)
    {
        std::memcpy(hash, _hash, SHA256_DIGEST_LENGTH);
        timestamp = _timestamp;
        length = _length;
    }
}
