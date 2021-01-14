all:
	g++ -pthread -I/usr/local/include/libxml2 -L/usr/local/lib -o Server/server Server/server.cpp -lxml2
	g++ Client/client.cpp -o Client/client
clean:
	rm -f *~Client/client *~Server/server
