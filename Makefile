BINS = encode decode playback server client
LIBS = libvoip.so

all : $(LIBS) $(BINS)

libvoip.so : voip.h voip.c
	gcc -Wall -shared -fPIC voip.c -lasound -o libvoip.so
encode: encode.c
	gcc encode.c -L/usr/local/lib -lspeex -o encode
decode: decode.c
	gcc decode.c -L/usr/local/lib -lspeex -o decode
playback: playback.c 
	gcc -g -Wall playback.c -lasound -lvoip -L$(PWD) -o playback
playback.o: playback.c
	gcc -c playback.c -lasound
server: server.c ring.o
	gcc server.c -pthread -lvoip -o server -L$(PWD)
client: client.c
	gcc client.c -pthread -o client

ring.o: ring.c ring.h
	gcc -c ring.c

clean:
	rm -f $(BINS)
	rm -f *o
	
