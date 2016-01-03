BINS = encode decode playback record server client
LIBS = libvoip.so

all : $(LIBS) $(BINS)

libvoip.so : voip.h voip.c
	gcc -Wall -shared -fPIC voip.c -lasound -o libvoip.so
encode: encode.c
	gcc encode.c -L/usr/local/lib -lspeex -o encode
decode: decode.c
	gcc decode.c -L/usr/local/lib -lspeex -o decode
playback: playback.c  ring.o
	gcc -g -Wall playback.c ring.o -lasound -lvoip -L$(PWD) -o playback
record: record.c 
	gcc -g -Wall record.c -lasound -lvoip -L$(PWD) -o record
playback.o: playback.c
	gcc -c playback.c -lasound
server: server.c ring.o
	gcc server.c -pthread ring.o -lvoip -lspeex -o server -L$(PWD)
client: client.c ring.o
	gcc client.c -pthread -o client -lvoip -lspeex ring.o -L$(PWD)

ring.o: ring.c ring.h
	gcc -c ring.c

clean:
	rm -f $(BINS)
	rm -f *o
	
