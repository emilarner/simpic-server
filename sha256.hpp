#pragma once

#include <string>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <openssl/sha.h>

#include "config.hpp"


#define SHA256_EXTENSION ".simpicsha256"

namespace SimpicServerLib
{
    /* A comparison for structure needed for std::map<K, V, Comp> to compare SHA256 hashes in its binary search tree. */
    struct SHA256Comparator
    {
        /* Returns true of sha1 greater than sha2. The order doesn't matter. What matters is that we can declare strings to be bigger or less than each other, for the binary search tree of std::map<K, V>. */
        bool operator()(const char *sha1, const char *sha2) const;
    };

    typedef char sha256_t;
    typedef char* sha256ptr_t;

    /* Calculate the SHA256 hash of the file opened by 'fs'. */
    /* Writes to a block of memory pointed to by 'where'. Make sure it can hold at least 32 bytes! */
    sha256ptr_t calculate_sha256(std::FILE *fp, sha256ptr_t where);

    struct SHA256CachedObject
    {
        sha256_t hash[SHA256_DIGEST_LENGTH];
        int64_t timestamp;
        uint64_t length;

        SHA256CachedObject(sha256ptr_t _hash, int64_t _timestamp, uint64_t _length);
    };
}