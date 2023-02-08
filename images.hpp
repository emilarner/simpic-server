#pragma once

#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <utility>
#include <tuple>
#include <cstdio>
#include <set>
#include <map>
#include <functional>
#include <optional>

#include <openssl/sha.h>
#include <png.h>
#include <jpeglib.h>

#include "sha256.hpp"
#include "phash/pHash.h"
#include "utils.hpp"

namespace SimpicServerLib
{
    const uint8_t JPEG_EXIF_MAGIC[12] = {
        0xFF, 0xD8, 0xFF, 0xE0, 
        0x00, 0x10, 0x4A, 0x46,
        0x49, 0x46, 0x00, 0x01
    };

    const uint8_t JPEG_MAGIC[4] = {
        0xFF, 0xD8, 0xFF, 0xDB
    };

    enum class ImageType
    {
        PNG,
        JPEG,
        GIF,
        Undefined // <~~ The file isn't a supported file type.
    };

    class Image
    {
    public:
        bool bad;

        uint16_t width;
        uint16_t height;

        uint32_t length;
        uint64_t phash;

        ImageType type;

        std::string filename;
        std::string path;
        std::string extension;
        
        sha256_t sha256[SHA256_DIGEST_LENGTH];

        /* Given an extension (without the .), return the image type. Note: this is a vulnerability/weakness to the simpic_server program--we aren't actually checking MAGIC numbers of the files to determine type, just the extension... perhaps we should just check the magic number? */
        static ImageType type_from_extension(const std::string &extension);

        /* Returns a pair containing the width and height of a .png file, from a C FILE* to it.*/
        static std::optional<std::pair<uint16_t, uint16_t>> get_png_dimensions(std::FILE *fp);

        /* Returns a pair containing the width and height of a .jpg/.jpeg file, from a C FILE* to it. */
        static std::optional<std::pair<uint16_t, uint16_t>> get_jpeg_dimensions(std::FILE *fp);

        /* Returns a uint64_t (an 8 byte, 64 bit) perceptual image hash of a path and a filename. */
        static uint64_t compute_perceptual_hash(std::string &path, std::string &filename);

        /* Group all similar images together. */
        static std::vector<std::vector<Image*>*> find_similar_images(std::vector<Image*> &images, 
                    uint8_t max_ham, std::function<void(int)> progress_callback);

        /* If there isn't a name to this Image, add a name by value. */
        void add_name(std::string name);

        /* Initializes an Image object, getting its extension, and file type. This does not get the image dimensions, nor does it get the perceptual hash. */
        Image(std::string _directory, std::string _filename, std::FILE *fp, sha256ptr_t hash);

		void set_location(std::string path, std::string filename);
        

        /* Bare bones initialization of an image. Useful if using as a structure moreso. */
        Image();

        /* Gets the information that the constructor of this object did not get, like the perceptual hash value and the dimensions of the image. This function returns nothing, but populates this class with the relevant information. It is more convenient to use C-style file handling. */
        bool get_info(std::FILE *fp);

        /* Useless function*/
        std::string abspath();
    };
}
