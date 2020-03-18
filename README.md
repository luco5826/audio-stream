# Audio stream

A simple software to stream mp3 audio from server to client.

## How to compile

This project depends on `libmpg123` for mp3 decoding and `libao` for audio playing.
You can install them using
```
sudo apt-get install libmpg123-dev libao-dev
```
Or by using your system's package manager.
Once you have all these dependencies, just run
```
make
```
in order to compile both files or `make client` `make server` to compile just one

## How to run

Once you have all the compiled files, you need to run the server using
```
./audio_server <mp3 file> <listen port> <buffer size>
```
Where `listen port` and `buffer size` will fallback to default values if missing
Then run the client with
```
./audio_client <server ip> <server port> <buffer size>
```
Where `server port` and `buffer size` will fallback to default values if missing