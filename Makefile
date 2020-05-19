make:
	make server
	make client
server:
	gcc server.c -pthread -o WTFServer
client:
	gcc client.c -lssl -lcrypto -pthread -o WTF
test:
	gcc test.c -o WTFTest
clean:
	rm WTFServer && rm WTF && rm WTFTest

	
	
