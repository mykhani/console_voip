#include <speex/speex.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#define FRAME_SIZE 160
 /* 160 bytes frame */

int main(int argc, char *argv[])
{
	FILE *fin, *fout;
	char *filename;
	short samples[FRAME_SIZE];
	SpeexBits bits;
	void *state; /* For holding encoder state */
	int quality;
	char bytes[FRAME_SIZE];
	int nbytes = 0; /* stores number of bytes in frame*/
	int tmp;

	if (argc<2) {
		printf("Please specify the compressed audio file\r\n");
		return EXIT_FAILURE;
	}
	
	filename = argv[1];
	fin = fopen(filename, "r");
	
	if (!fin) {
		printf("Unable to open file %s : %s \r\n", filename, strerror(errno));
		return EXIT_FAILURE;
	}
	/* Create encoder state in narrowband mode */
	state = speex_decoder_init(&speex_nb_mode);
	
	/* Set the perceptual enhancement ON */
	tmp = 1;
	speex_decoder_ctl(state, SPEEX_SET_ENH, &tmp);
	
	speex_bits_init(&bits);

	fout = fopen("audio.dec", "w");

	if (!fout) {
		printf("Unable to write file audio.dec : %s \r\n", strerror(errno));
	}

	while (1) {

		/* Read the size of encoded frame */
		fread(&nbytes, sizeof(int), 1, fin);
		if(feof(fin))
			break;
		
		fread(bytes, sizeof(char), nbytes, fin);

		printf("Number of bytes to de-compress: %d \n", nbytes);		
		
		speex_bits_reset(&bits);

		speex_bits_read_from(&bits, bytes, nbytes);

		/* Decode here */
		speex_decode_int(state, &bits, samples);
		/* Write decoded samples to output file */
		fwrite(samples, sizeof(short), FRAME_SIZE, fout);
	}
	
free: 
	printf("Freeing up resources \r\n");
	/* Destroy the encoder state */
	speex_decoder_destroy(state);
	/* Destroy the bits-packing */
	speex_bits_destroy(&bits);
	fclose(fin);
	fclose(fout);
	return 0;

}
