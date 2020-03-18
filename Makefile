CC=$(CROSS)gcc
PKG_CONFIG=$(CROSS)pkg-config

all:
	make server
	make client
server:
	$(CC) audio_server.c -o audio_server -lmpg123
client:
	$(CC) audio_client.c -o audio_client -lao
windows:
	$(CC) audio_client.c -o audio_client.exe  `$(CROSS)pkg-config --libs ao` -lws2_32

