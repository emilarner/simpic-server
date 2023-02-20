#include "simpic_server.hpp"

namespace SimpicServerLib
{
	SimpicServer::SimpicServer(uint16_t _port, const std::string &simpic_dir, const std::string &_recycle_bin)
	{
		std::cout << "Simpic server successfully initialized. " << std::endl;
		recycle_bin_on = _recycle_bin != "";
		recycle_bin = _recycle_bin;
		alt_tmp = simpic_dir + "tmp/";
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

				/* A check requires more information. */
				if (req.request == (uint8_t)ClientRequests::Check ||
					req.request == (uint8_t)ClientRequests::CheckRecursive)
				{
					uint16_t ccreq_no = 0;
					recvall(client->fd, &ccreq_no, sizeof(ccreq_no));

					struct ClientCheckRequest ccreq;

					for (int i = 0; i < ccreq_no; i++)
					{
						recvall(client->fd, &ccreq, sizeof(ccreq));

						switch ((ClientCheckRequestTypes)ccreq.method)
						{
							/* The client is going to send us raw file data. */
							case ClientCheckRequestTypes::ByData:
							{
								std::string tmp_path = alt_tmp + "/" + random_chars(16);
								std::ofstream of(tmp_path, std::ios::binary);

								char buffer[BUFFER_SIZE];
								int whole = ccreq.length / sizeof(buffer);
								int frac = ccreq.length % sizeof(buffer);

								for (int j = 0; j < whole; j++)
								{
									recvall(client->fd, buffer, sizeof(buffer));
									of.write(buffer, sizeof(buffer));
								}

								recvall(client->fd, buffer, frac);
								of.write(buffer, frac);
								of.close();

								client->check_files.push_back(tmp_path);
								break;
							}

							case ClientCheckRequestTypes::ByPath:
							{
								char *cc_path = new char[ccreq.length];
								recvall(client->fd, cc_path, ccreq.length);
								cc_path[ccreq.length - 1] = '\0';

								client->check_files.push_back(std::string(cc_path));

								delete[] cc_path;
								break;
							}

							case ClientCheckRequestTypes::ByPHash:
							{
								uint64_t phash_val;
								recvall(client->fd, &phash_val, sizeof(phash_val));
								client->check_files_dct_phash.push_back(phash_val);

								break;
							}
						}
					}
				}

				bool recursive = (req.request == (uint8_t)ClientRequests::CheckRecursive ||
								req.request == (uint8_t)ClientRequests::CacheRecursive ||
									req.request == (uint8_t)ClientRequests::ScanRecursive);

				/* The client's request. */
				switch ((ClientRequests) req.request)
				{
					case ClientRequests::Exit:
						goto cleanup;

					case ClientRequests::Check:
					case ClientRequests::Cache:
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
						if ((error = 
							client->simpic_in_directory(ppath, (ClientRequests)req.request, req.max_ham)) 
							!= 0)
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
}
