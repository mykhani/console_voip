all : encode decode playback server

encode: encode.c
	gcc encode.c -L/usr/local/lib -lspeex -o encode
decode: decode.c
	gcc decode.c -L/usr/local/lib -lspeex -o decode
playback: playback.c
	gcc playback.c -lasound -o playback
server: server.c
	gcc server.c -pthread -o server
