CC=g++
LIBS=-L$(shell pwd) -lpthread -lpHash -ljpeg -ltiff -lpng -lssl -lcrypto -lsimpicserver
CPPFLAGS=-g -std=c++20


simpic_server: libsimpicserver.so main.o testing/test_simpic_alg testing/test_child_node_alg simpic_protocol.hpp
	$(CC) $(CPPFLAGS) -o simpic_server main.o $(LIBS)

testing/test_simpic_alg: libsimpicserver.so testing/test_simpic_alg.o
	$(CC) $(CPPFLAGS) -o testing/test_simpic_alg images.o utils.o testing/test_simpic_alg.o simpic_cache.o sha256.o $(LIBS)

testing/test_child_node_alg: libsimpicserver.so testing/test_child_node_alg.o
	$(CC) $(CPPFLAGS) -o testing/test_child_node_alg testing/test_child_node_alg.o $(LIBS)

libsimpicserver.so: images.o networking.o simpic_cache.o simpic_server.o utils.o sha256.o simpic_client.o
	$(CC) $(CPPFLAGS) -shared -o libsimpicserver.so images.o networking.o simpic_cache.o simpic_server.o utils.o sha256.o simpic_client.o $(LIBS)


testing/test_simpic_alg.o: testing/test_simpic_alg.cpp
	$(CC) $(CPPFLAGS) -o testing/test_simpic_alg.o -c testing/test_simpic_alg.cpp

testing/test_child_node_alg.o: testing/test_child_node_alg.cpp
	$(CC) $(CPPFLAGS) -o testing/test_child_node_alg.o -c testing/test_child_node_alg.cpp

sha256.o: sha256.cpp
	$(CC) $(CPPFLAGS) -fPIC -c sha256.cpp

main.o: main.cpp
	$(CC) $(CPPFLAGS) -c main.cpp

images.o: images.cpp images.hpp
	$(CC) $(CPPFLAGS) -fPIC -c images.cpp

networking.o: networking.cpp networking.hpp
	$(CC) $(CPPFLAGS) -fPIC -c networking.cpp

simpic_cache.o: simpic_cache.cpp simpic_cache.hpp
	$(CC) $(CPPFLAGS) -fPIC -c simpic_cache.cpp

simpic_server.o: simpic_server.cpp simpic_server.hpp
	$(CC) $(CPPFLAGS) -fPIC -c simpic_server.cpp

simpic_client.o: simpic_client.cpp simpic_client.hpp
	$(CC) $(CPPFLAGS) -fPIC -c simpic_client.cpp

utils.o: utils.cpp utils.hpp
	$(CC) $(CPPFLAGS) -fPIC -c utils.cpp


install: simpic_server
	mkdir -p /usr/include/simpic_server/
	mkdir -p /usr/include/simpic_server/phash/
	cp phash/pHash.h /usr/include/simpic_server/phash/

	cp simpic_server /usr/bin/
	chmod 0755 /usr/bin/simpic_server

	cp libsimpicserver.so /usr/lib/
	chmod 0755 /usr/lib/libsimpicserver.so

	cp *.hpp /usr/include/simpic_server/
	chmod -R 0755 /usr/include/simpic_server/ 

clean:
	rm *.o
	rm simpic_server
	rm testing/test_simpic_alg.o
	rm testing/test_simpic_alg
	rm testing/test_child_node_alg.o
	rm testing/test_child_node_alg
	rm libsimpicserver.so