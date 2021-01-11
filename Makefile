all:
	g++ -pthread Server/server.cpp -o Server/server
	g++ Client/client.cpp -o Client/client
clean:
	rm -f *~Client/client *~Server/server