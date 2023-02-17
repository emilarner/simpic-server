#include "simpic_server.hpp"

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
		std::string new_path = recycling_bin + filename + "_" + random_chars(RANDOM_CHARS_LENGTH);

		if (rename(absolute_path.c_str(), new_path.c_str()) < 0)
		{
			std::cerr << "[" << to_string() << "]: failure moving: " << std::strerror(errno) << "\n";
			return;
		}

		moving_log->write((std::string)"Moved" + absolute_path + (std::string)" to " + new_path);
	}

	SimpicServer::SimpicServer(uint16_t _port, const std::string &simpic_dir, const std::string &_recycle_bin)
	{
		std::cout << "Simpic server successfully initialized. " << std::endl;
		recycle_bin_on = _recycle_bin != "";
		recycle_bin = _recycle_bin;
		port = _port;

		/* Initialize and read everything into the cache, if it is present. */
		cache = new SimpicCache(simpic_dir + "cache.simpic_cache");
		cache->readall();
		std::cout << "Cache successfully initialized." << std::endl;

		new_moving_log.open("/var/log/simpic_moving_log");
		new_moving_log.open(simpic_dir + "moving_log");

		new_activity_log.open("/var/log/simpic_log");
		new_activity_log.open(simpic_dir + "log");

		on_ready = []() -> void {};
	}

	void SimpicServer::save_cache()
	{
		cache->saveall();
	}

	SimpicServer::SimpicServer(uint16_t _port)
	{
		port = _port;
		recycle_bin_on = false;
	}

	void SimpicServer::start()
	{
		std::cout << "Simpic server started." << std::endl;

		signal(SIGPIPE, SIG_IGN);

		/* Make a TCP/IPv4 socket. */
		fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

		sock.sin_family = AF_INET;

		/* Convert the port (uint16_t) to Network Byte Order (Big Endian) */
		sock.sin_port = htons(port);
		sock.sin_addr.s_addr = INADDR_ANY;

		int value = 1;

		/* Make it so that the ports don't get clogged. */
		if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(int)) < 0)
		{
			std::cerr << "setsockopt(SO_REUSEADDR) failed: " << std::strerror(errno) << "\n";
			exit(-1);
		}

		/* If binding failed to the specified port. */
		if ((bind(fd, (struct sockaddr*) &sock, sizeof(sock))) < 0)
		{
			std::cerr << "Failed to bind socket on port " << port << ": " << std::strerror(errno) << "\n";
			exit(-1);
		}

		std::cout << "Simpic server bound to port " << port << "\n";

		listen(fd, 64);

		std::cout << "Simpic server now listening for connections.\n";

		/* A callback for when the Simpic server is ready. */
		on_ready();

		while (true)
		{
			struct sockaddr_in client;
			socklen_t client_len = sizeof(client);

			int cfd = 0;

			/* Try to accept the client and then read its data into the structure, also getting its file descriptor. If it errors, print out the reason why and exit the program. */
			if ((cfd = accept(fd, (struct sockaddr*) &client, &client_len)) < 0)
			{
				std::cerr << "Failed to accept a client on port " << port << ": " << std::strerror(errno) << "\n";
				exit(-2);
			}

			/* The client object needs to transcend the stack, so we need to heap allocate it. */
			SimpicClient *sc = new SimpicClient(cache, recycle_bin, &new_activity_log, &new_moving_log);
			
			sc->addr = client;
			sc->fd = cfd;
			sc->recycling_bin = recycle_bin;

			std::cout << "Client " << sc->to_string() << " connected!" << "\n";
			new_activity_log.write((std::string)"Client " + sc->to_string() + (std::string)" connected!");

			/* It belongs on another thread. */
			std::thread th(&SimpicServer::handler, this, sc);
			th.detach();
		}	
	}

	void SimpicServer::handler(SimpicClient *client)
	{
		signal(SIGPIPE, SIG_IGN);

		char *path = nullptr;

		try 
		{
			while (true)
			{
				/* Take the client's initial request--do they want to scan, scan recursively, or exit? */
				struct ClientRequest req;
				recvall(client->fd, &req, sizeof(req));

				/* A VLA would not work here, so we have to allocate memory... and we're doing it the C++ way! */
				/*if (req.path_length == 0 || req.path_length > 4096)
				{
					struct MainHeader hdr;
					hdr.code = (uint8_t) MainHeaderCodes::Limits; // way too big
					sendall(client->fd, &hdr, sizeof(hdr));

					char limits_msg[8] = {0};
					memset(limits_msg, 0, sizeof(limits_msg));
					strcpy(limits_msg, "path");
					sendall(client->fd, limits_msg, sizeof(limits_msg));

					continue;
				}*/

				if (req.path_length != 0)
				{
					path = new char[req.path_length];

					/* Possible vulnerability here, but this program isn't supposed */
					/* to be available to the wider internet... so does it even matter? This is also overkill. */
					recvall(client->fd, path, req.path_length);

					/* A null-terminator is never guaranteed, so put one just in case where the client */
					/* said it should be. */
					path[req.path_length - 1] = '\0';
				}

				bool recursive = false;

				/* The client's request. */
				switch ((ClientRequests) req.request)
				{
					case ClientRequests::Exit:
					{
						goto cleanup;
						return;
					}

					case ClientRequests::ScanRecursive:
						recursive = true;

					case ClientRequests::Scan:
					{
						struct MainHeader mh;
						std::string ppath(path);

						/* Another client is already doing this. */
						/* When recursive mode becomes supported, this algorithm will be harder. */

						client_mutex.lock();

						/* If we have a directory conflict*/
						bool exists = false;

						for (std::pair<std::string, bool> item : active_folders)
						{
							/* If they're equal, it*/
							/* definitely is a conflict. */
							if (item.first == ppath)
							{
								exists = true;
								break;
							}

							/* If the requested path is a child node of an existing */
							/* directory and that existing directory is being scanned */
							/* recursively, NO! */
							if (dir_is_child(item.first, ppath) && item.second)
							{
								exists = true;
								break; 
							}
						}

						if (exists)
						{
							/* Report such a crippling error. */
							mh.code = (uint8_t) MainHeaderCodes::DirectoryAlreadyActive;
							mh._errno = 0;
							mh.set_no = -1;
							sendall(client->fd, &mh, sizeof(mh));
							goto cleanup;
						}
						
						client_mutex.unlock();

						int error = 0;


						client_mutex.lock();
						active_folders.insert({ppath, recursive});
						client_mutex.unlock();

						/* Call the function that handles this. If it exited with 0, an error occured! */
						if ((error = client->simpic_in_directory(ppath, recursive, req.max_ham)) != 0)
						{
							/* An error of -1 indicates the connection died. */
							if (error != -1)
							{
								mh.code = (uint8_t) MainHeaderCodes::Failure;
								mh._errno = error;
								mh.set_no = -1;

								sendall(client->fd, &mh, sizeof(mh));
							}
						}

						client_mutex.lock();
						active_folders.erase({ppath, recursive});
						client_mutex.unlock();

						/* If there was an error, escape from the loop and free resources. */
						if (error != 0)
							goto cleanup;

						break;
					}
				}

				delete path;
				path = nullptr;
			} 
		}
		catch (simpic_networking_exception &ex)
		{
			std::cerr << "[" << client->to_string() << "]: " << "Networking error occured: " << ex.what() << std::endl;
		}
		catch (std::exception &ex)
		{
			std::cerr << "[" << client->to_string() << "]: " << "General exception occured: " << ex.what();
			std::cerr << "\n";
		}

cleanup:
		/* Protect against freeing null pointer. */
		if (path != nullptr)
			delete path;
		
		if (client != nullptr)
		{
			close(client->fd);
			delete client;
		}

		return;
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
				//std::ifstream reading(pic->abspath());
				std::FILE *fp = std::fopen(pic->abspath().c_str(), "rb");
				std::fseek(fp, 0, SEEK_END);
				size_t file_size = std::ftell(fp);
				std::fseek(fp, 0, SEEK_SET);
				
				size_t amnt = 0;
				char buffer[BUFFER_SIZE];

				new_sendfile(fd, fileno(fp), file_size);
				

				std::fclose(fp);
				//sendfile()
				//reading.close();
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

	int SimpicClient::simpic_in_directory(std::string &dir, bool recursive, uint8_t max_ham)
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

				if (!recursive)
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
				std::cerr << "(" << to_string() << "): Error opening (valid?) file (" << absname << "): " << std::strerror(errno) << std::endl;
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

		int total_images = 0;

		struct UpdateHeader uh;
		std::memset(&uh, 0, sizeof(uh));

		std::vector<std::vector<Image*>*> results = Image::find_similar_images(imgs, max_ham, [this, &uh](int x){
			uh.images = x;

			if (x % UPDATE_INCREMENTS == 0)
			{
				send(this->fd, &uh, sizeof(uh), MSG_DONTWAIT);
				//sendall(this->fd, &uh, sizeof(uh));
			}
		});
		
		uh.done = true;
		//sendall(fd, &uh, sizeof(uh));
		send(fd, &uh, sizeof(uh), MSG_DONTWAIT);

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

	end:
		cache->saveall();
		return 0;
	}
}
