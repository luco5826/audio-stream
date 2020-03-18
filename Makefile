all:
	make server
	make client
server:
	gcc audio_server.c -o audio_server -lmpg123
client:
	gcc audio_client.c -o audio_client -lao