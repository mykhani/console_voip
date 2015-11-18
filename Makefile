all : encode decode

encode :
	gcc encode.c -L/usr/local/lib -lspeex -o encode
decode :
	gcc decode.c -L/usr/local/lib -lspeex -o decode

