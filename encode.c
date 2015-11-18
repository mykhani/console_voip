#include <speex/speex.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#define FRAME_SIZE 160 /* 160 bytes frame */

int main(int argc, char *argv[])
{
	FILE *fin, *fout;
	char *filename;
	short samples[FRAME_SIZE];
	SpeexBits bits;
	void *state; /* For holding encoder state */
	int quality;
	char bytes[200];
	int nbytes = 0; /* stores total number of bytes */
	int tmp;

	if (argc<2) {
		printf("Please specify the input audio file\r\n");
		return EXIT_FAILURE;
	}
	
	filename = argv[1];
	fin = fopen(filename, "r");
	
	if (!fin) {
		printf("Unable to open file %s : %s \r\n", filename, strerror(errno));
		return EXIT_FAILURE;
	}
	/* Create encoder state in narrowband mode */
	state = speex_encoder_init(&speex_nb_mode);
	
	/* Set the quality to 8 (15 kbps) */
	quality = 8;
	speex_encoder_ctl(state, SPEEX_SET_QUALITY, &quality);

	speex_bits_init(&bits);

	fout = fopen("audio.enc", "w");

	if (!fout) {
		printf("Unable to write file audio.enc : %s \r\n", strerror(errno));
	}

	while (1) {
		/* Read the frame of samples from audio file */
		fread(samples, sizeof(short), FRAME_SIZE, fin);
		if(feof(fin))
			break;
		
		speex_bits_reset(&bits);

		/* Encode here */
		speex_encode_int(state, samples, &bits);
		/* Write the encoded bits to array of bytes so that they can be written */
		nbytes = speex_bits_write(&bits, bytes, 200);
	
		/* Write the size of encoded frame first */
		fwrite(&nbytes, sizeof(int), 1, fout);

		/* Write the encoded frame itself*/
		fwrite(bytes, sizeof(char), nbytes, fout);
	}
	
free: 
	printf("Freeing up resources \r\n");
	/* Destroy the encoder state */
	speex_encoder_destroy(state);
	/* Destroy the bits-packing */
	speex_bits_destroy(&bits);
	fclose(fin);
	fclose(fout);
	return 0;

}
