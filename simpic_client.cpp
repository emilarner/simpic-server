#include "simpic_client.hpp"

namespace SimpicServerLib
{
    SimpicClient::SimpicClient(SimpicCache *_cache, const std::string &recycle_bin, 
			Logger *main, Logger *moving)
	{
		cache = _cache;
		recycling_bin = recycle_bin;
		main_log = main;
		moving_log = moving;
	}

	void SimpicClient::deal_with_file(const std::string &path, const std::string &filename)
	{
		std::string absolute_path = path + std::string("/") + filename;
		std::string new_path = recycling_bin + random_chars(RANDOM_CHARS_LENGTH) + "_" + filename;

		if (rename(absolute_path.c_str(), new_path.c_str()) < 0)
		{
			std::cerr << "[" << to_string() << "]: failure moving: " << std::strerror(errno) << "\n";
			return;
		}

		moving_log->write((std::string)"Moved" + absolute_path + (std::string)" to " + new_path);
	}

    std::string SimpicClient::to_string()
	{
		std::string result;

		result += std::string(inet_ntoa(this->addr.sin_addr));
		result += " : ";
		result += std::to_string(ntohs(this->addr.sin_port));

		return result;
	}

	void SimpicClient::set_of_pics(std::vector<Image*> *pics)
	{
		/* Tell the client that we are sending a set of pictures. */
		struct SetHeader sethdr;
		sethdr.count = pics->size();
		sethdr.type = (uint8_t) DataTypes::Image;
		sendall(fd, &sethdr, sizeof(sethdr));

		for (Image *pic : *pics)
		{
			/* Give valuable information about each picture. */
			struct ImageHeader imghdr;

			imghdr.filename_length = pic->filename.size() + 1;
			std::memcpy(imghdr.sha256_hash, pic->sha256, sizeof(imghdr.sha256_hash));
			// inefficient ~~~^

			imghdr.size = pic->length;
			
			imghdr.path_length = pic->path.size() + 1;
			imghdr.width = pic->width;
			imghdr.height = pic->height;

			sendall(fd, &imghdr, sizeof(imghdr));

			/* Send the filename and then the path. */
			sendall(fd, (char*) pic->filename.c_str(), imghdr.filename_length);
			sendall(fd, (char*) pic->path.c_str(), imghdr.path_length);

			
			/* Receive the plea from the client which states what else they want from us. */
			struct ClientPlea plea;
			recvall(fd, &plea, sizeof(plea));

			/* If the client didn't make a plea for no data... Self-explanatory.*/
			if (!plea.no_data)
			{
				std::FILE *fp = std::fopen(pic->abspath().c_str(), "rb");
				std::fseek(fp, 0, SEEK_END);
				size_t file_size = std::ftell(fp);

				std::fseek(fp, 0, SEEK_SET);
				

				new_sendfile(fd, fileno(fp), file_size);
			
				std::fclose(fp);
			}
		}

		struct ClientAction act;
		recvall(fd, &act, sizeof(act));

		/* Blocks... it *waits* */

		/* Don't do anything, keep them all. */
		if (act.action == (uint8_t)ClientActions::Keep)
			return;


		/* Read all of the indices of the files to delete/deal with. */
		uint8_t indices[act.deletions];
		recvall(fd, indices, act.deletions);

		/* Deal with every index. If it is invalid, skip over it and complain. */
		for (int i = 0; i < act.deletions; i++)
		{
			uint8_t index = indices[i];

			try 
			{
				Image *img = (*pics)[index];
				deal_with_file(img->path, img->filename);
			}
			catch (std::out_of_range &ex)
			{
				std::cerr << "Incorrect index given in set_of_pics(): " << index << "\n";
			}
		}
	}

	int SimpicClient::simpic_in_directory(const std::string &dir, ClientRequests req, uint8_t max_ham)
	{
		std::vector<Image*> imgs;
		std::map<sha256ptr_t, Image*, SHA256Comparator> hash2img;

		DIR *d = opendir(dir.c_str());

		if (d == nullptr)
		{
			int error = errno;
			std::cerr << "Error opening directory '" << dir << "': " << std::strerror(error) << "\n";
			return error;
		}

		/* Go through every file in the directory given, using C's <dirent.h> interface. */
		for (struct dirent *ent = readdir(d); ent != nullptr; ent = readdir(d))
		{
			/* We have no business with other types of files. */
			if (ent->d_type != DT_DIR && ent->d_type != DT_REG)
				continue;

			/* This is a directory, obviously... */
			if (ent->d_type == DT_DIR)
			{
				/* Special directories we don't want to traverse. */
				if (ent->d_name[0] == '.')
					continue;

				if (req != ClientRequests::ScanRecursive || req != ClientRequests::CheckRecursive)
					continue;


				continue;
			}

			std::string cpp_name(ent->d_name);
			
			SimpicEntryTypes type = SimpicCache::get_type_from_extension(get_extension(cpp_name));

			/* Unsupported type. */
			if (type == SimpicEntryTypes::Undefined)
				continue;

			/* C-style file handling works more nicely with the libraries we're using. */
			/* ... plus I think it's better than <fstream>. */

			std::string absname = dir + "/" + std::string(ent->d_name);
			std::FILE *fp = std::fopen(absname.c_str(), "rb");

			/* The file didn't open for some reason? */
			if (fp == nullptr)
			{
				std::cerr << "(" << to_string() << "): Error opening (valid?) file (" << absname << "): " 
					<< std::strerror(errno) << std::endl;

				continue;
			}

			struct stat fileinfo;
			stat(absname.c_str(), &fileinfo);

			sha256_t image_hash_buffer[SHA256_DIGEST_LENGTH];
			SHA256CachedObject *sha256_obj = nullptr;

			/* Attempt to pull the SHA256 hash from the cache. */
			if ((sha256_obj = cache->get_sha256(absname, fileinfo.st_size, fileinfo.st_mtim.tv_sec)) 
					== nullptr)
			{
				calculate_sha256(fp, image_hash_buffer);

				sha256_obj = new SHA256CachedObject(
					image_hash_buffer,
					fileinfo.st_mtim.tv_sec,
					fileinfo.st_size
				);

				/* Update the cache with a new one. */
				cache->insert({absname, sha256_obj});
			}

			sha256ptr_t image_hash = sha256_obj->hash;

			switch (type) 
			{
				case SimpicEntryTypes::Image: 
				{
					Image *img = nullptr;

					/* The image does not exist in the cache: make one, then put it into the cache. */
					if ((img = cache->get_image(image_hash)) == nullptr)
					{
						img = new Image(dir, cpp_name, fp, image_hash);

						/* If it does not have the magic or does not pass the test. */
						/* This isn't very expensive, because it's actually really rare that this */
						/* would happen. */
						if (!img->get_info(fp))
						{
							std::fclose(fp);
							delete img;

							continue;
						}

						cache->insert(img);
					}
			
					img->filename = cpp_name;
					img->path = dir;

					imgs.push_back(img);
					hash2img[img->sha256] = img;
					break;
				}
			}

			std::fclose(fp);
		}

		cache->saveall();
		closedir(d);

		/* If caching, no further actions need to be done. */
		if (req == ClientRequests::Cache || req == ClientRequests::CacheRecursive)
		{
			struct MainHeader hdr;
			hdr.set_no = -1;
			hdr._errno = 0;
			hdr.code = (uint8_t) MainHeaderCodes::Success;

			sendall(fd, &hdr, sizeof(hdr));
			return 0;
		}

		int total_images = 0;

		struct UpdateHeader uh;
		std::memset(&uh, 0, sizeof(uh));

		if (req == ClientRequests::Check || req == ClientRequests::CheckRecursive)
		{
			std::vector<Image*> needles;
			for (const std::string &path : check_files)
			{
				std::FILE *fp = std::fopen(path.c_str(), "rb");
				sha256_t ndl_hash[SHA256_DIGEST_LENGTH];
				calculate_sha256(fp, (sha256ptr_t)ndl_hash);

				Image *ndl_img = nullptr;
				if ((ndl_img = cache->get_image(ndl_hash)) == nullptr)
				{
					ndl_img = new Image(
						(const std::string)path,
						(const std::string)path,
						fp,
						ndl_hash
					);

					if (!ndl_img->get_info(fp))
					{
						delete ndl_img;
						goto check_explicit_cleanup;
					}
				}

				needles.push_back(ndl_img);

check_explicit_cleanup:
				std::fclose(fp);
			}

			for (uint64_t ndl_hash : check_files_dct_phash)
			{
				Image *ndl_img = new Image();
				ndl_img->phash = ndl_hash;
				needles.push_back(ndl_img);
			}

			std::vector<std::vector<Image*>*> results = Image::find_duplicates(
				imgs,
				needles,
				max_ham
			);

			struct MainHeader mhdr;
			mhdr.code = (uint8_t)MainHeaderCodes::Success;
			mhdr.set_no = (uint8_t)results.size();
			mhdr._errno = 0;

			sendall(fd, &mhdr, sizeof(mhdr));

			for (std::vector<Image*>* result : results)
			{
				set_of_pics(result);
				delete result;
			}
		}

		if (req == ClientRequests::Scan || req == ClientRequests::ScanRecursive)
		{
			std::vector<std::vector<Image*>*> results = Image::find_similar_images(imgs, max_ham, [this, &uh](int x){
				uh.images = x;

				if (x % UPDATE_INCREMENTS == 0)
					sendall(this->fd, &uh, sizeof(uh));
				
			});
			
			uh.done = true;
			sendall(fd, &uh, sizeof(uh));

			int total = results.size();

			try 
			{
				/* No results were found--tell the client that! */
				if (!total)
				{
					struct MainHeader hdr;
					hdr.code = (uint8_t)MainHeaderCodes::NoResults;
					sendall(fd, &hdr, sizeof(hdr));
					cache->saveall();
					return 0;
				}

				/* We need to tell the client how many results we've gotten. */
				struct MainHeader hdr;
				hdr.code = (uint8_t) MainHeaderCodes::Success;
				hdr._errno = 0;
				hdr.set_no = total;
				sendall(fd, &hdr, sizeof(hdr));

				/* Serve the client with a set of pictures that we've ascertained are close. */
				/* Also prevent a memory leak by freeing the memory. */
				for (std::vector<Image*>* set : results) 
				{
					set_of_pics(set);
					delete set;
				}
			}
			catch (simpic_networking_exception &ex)
			{
				std::cerr << "(" << to_string() << "): Network error: " << ex.what() << std::endl;
				cache->saveall();
				return -1;
			}
		}

	end:
		cache->saveall();
		return 0;
	}
}