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
        std::fseek(fp, 0, SEEK_SET);
        return where;
    }
}
