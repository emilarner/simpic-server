#include "images.hpp"

namespace SimpicServerLib 
{
    uint64_t Image::compute_perceptual_hash(std::string &path, std::string &filename)
    {
        uint64_t value;
        ph_dct_imagehash(concatenate_folder(path, filename).c_str(), value);
        return value; 
    }

    ImageType Image::type_from_extension(const std::string &extension)
    {
        if (extension == "png")
            return ImageType::PNG;

        if (extension == "jpg" || extension == "jpeg")
            return ImageType::JPEG;

        return ImageType::Undefined;
    }


    std::optional<std::pair<uint16_t, uint16_t>> Image::get_png_dimensions(std::FILE *fp)
    {
        uint32_t width;
        uint32_t height;

        png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);

        if (png == nullptr)
            return std::nullopt;

        png_infop info = png_create_info_struct(png);

        if (info == nullptr)
            return std::nullopt;

        png_init_io(png, fp);
        png_read_info(png, info);

        width = png_get_image_width(png, info);
        height = png_get_image_height(png, info);

        png_destroy_read_struct(&png, &info, nullptr);
        return std::make_pair<uint16_t, uint16_t>((uint16_t) width, (uint16_t) height);
    }

    std::optional<std::pair<uint16_t, uint16_t>> Image::get_jpeg_dimensions(std::FILE *fp)
    {
        /* Read in the magic. */
        char enough[sizeof(JPEG_EXIF_MAGIC)];
        std::fseek(fp, 0, SEEK_SET);
        std::fread(enough, 1, sizeof(enough), fp);

        /* Check if the magic aligns with a valid JPEG file. There are generally two magics. */
        /* If it doesn't, return a null value. */
        if (!!std::memcmp(enough, JPEG_EXIF_MAGIC, sizeof(JPEG_EXIF_MAGIC)) || 
                !!std::memcmp(enough, JPEG_MAGIC, sizeof(JPEG_MAGIC)))
        {
            return std::nullopt; // null std::optional<T> value. 
        }

        std::fseek(fp, 0, SEEK_SET);

        uint32_t height;
        uint32_t width;

        struct jpeg_decompress_struct jpeginfo;
        jpeg_create_decompress(&jpeginfo);
        jpeg_stdio_src(&jpeginfo, fp);

        jpeg_read_header(&jpeginfo, true);

        height = jpeginfo.image_height;
        width = jpeginfo.image_width;

        jpeg_finish_decompress(&jpeginfo);
        return std::make_pair<uint16_t, uint16_t>((uint16_t) width, (uint16_t) height);
    }


    std::vector<std::vector<Image*>*> Image::find_similar_images(std::vector<Image*> &images, 
            uint8_t max_ham, std::function<void(int)> progress_callback)
    {
        std::unordered_map<int, std::vector<Image*>*> results;
        std::unordered_set<int> seen;

        /* Generate non-repeating permutations of images. */
        int count = 0;
        for (int i = 0; i < images.size(); i++)
        {
            Image *current = images[i];

            /* We don't need to over this, since we already did below... */
            if (seen.find(i) != seen.end())
                continue;

            results[i] = new std::vector<Image*>();
            results[i]->push_back(current);


            for (int j = i + 1; j < images.size(); j++)
            {
                /* Again, let's not repeat things.*/
                if (seen.find(j) != seen.end())
                    continue;

                /* The SHA256 hashes of the files are the same, therefore they are duplicates. */
                if (!std::memcmp(current->sha256, images[j]->sha256, SHA256_DIGEST_LENGTH))
                {
                    seen.insert(j);
                    //image_hams[current->sha256]->push_back(images[j]);
                    results[i]->push_back(images[j]);
                    continue;
                }

                uint8_t hamming_distance = ph_hamming_distance(current->phash, images[j]->phash);

                if (hamming_distance > max_ham)
                    continue;


                count++;

                results[i]->push_back(images[j]);
                seen.insert(j);
            }

            if (count != 0)
                progress_callback(count);
                
            seen.insert(i);
        }

        std::vector<std::vector<Image*>*> result;

        for (auto &[key, value] : results)
        {
            if (value->size() < 2)
                continue;

            result.push_back(value);
        }

        return result;
    }

    void Image::add_name(std::string name)
    {
        filename = name;
    }

    /* To make the linker happy. */
    Image::Image()
    {

    }

    Image::Image(std::string _directory, std::string _filename, FILE *fp, sha256ptr_t hash)
    {
        height = 0;
        length = 0;

        bad = false;
        path = _directory;

        filename = _filename;
        extension = get_extension(_filename);
        type = Image::type_from_extension(extension);

        std::fseek(fp, 0, SEEK_END);
        length = std::ftell(fp);
        std::fseek(fp, 0, SEEK_SET);

        std::memcpy(sha256, hash, SHA256_DIGEST_LENGTH);
    }

    bool Image::get_info(std::FILE *fp)
    {
        switch (type)
        {
            case ImageType::PNG:
            {
                std::optional<std::pair<uint16_t, uint16_t>> dims = Image::get_png_dimensions(fp);

                if (!dims)
                    return false;

                std::tie(this->width, this->height) = *dims;
                break;
            }

            case ImageType::JPEG:
            {
                std::optional<std::pair<uint16_t, uint16_t>> dims = Image::get_jpeg_dimensions(fp);

                if (!dims)
                    return false;

                std::tie(this->width, this->height) = *dims;
                break;
            }

            default:
            {
                bad = true;
                return false;
            }
        }

        phash = Image::compute_perceptual_hash(path, filename);
        return true;
    }

    std::string Image::abspath()
    {
        return concatenate_folder(path, filename);
    }
}
