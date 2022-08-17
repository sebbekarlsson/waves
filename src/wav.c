#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <waves/wav.h>
#include <limits.h>

int wav_write(Wave wave, const char* path) {
  WavHeader header = {};

  FILE *out = fopen(path, "wb");
  if (out == NULL) {
    printf("Could not write to %s\n", path);
    return 0;
  }


  printf("Writing file: %s\n", path);

  fwrite(&wave.header, sizeof(header), 1, out);

  fseek(out, 0, SEEK_END);

  fwrite(&wave.data[0], sizeof(&wave.data[0]), wave.length, out);

  fclose(out);

  printf("Done, wrote file %s\n", path);

  return 1;
}


int wav_read(Wave* wave, const char *path) {

  if (access(path, F_OK) != 0) {
    failed_to_read:
    printf("could not open `%s`\n", path);
    return 0;
  }

  FILE *fp = fopen(path, "rb");

  if (!fp) {
    goto failed_to_read;
  }

  WavHeader* h = &wave->header;

  fread(h->riff, sizeof(uint8_t), 4, fp);
  fread(&h->overall_size, sizeof(uint32_t), 1, fp);
  fread(h->wave, sizeof(uint8_t), 4, fp);

  // skip junk if any
  while (memcmp(h->fmt_chunk_marker, "fmt ", 4) != 0) {
    if ((fread(h->fmt_chunk_marker, sizeof(uint8_t), 4, fp)) == EOF) {
      printf("Error reading wav file.\n");
      fclose(fp);
      return 0;
    }
  }

  fread(&h->length_of_fmt, sizeof(uint32_t), 1, fp);
  fread(&h->format_type, sizeof(uint16_t), 1, fp);
  fread(&h->channels, sizeof(uint16_t), 1, fp);
  fread(&h->sample_rate, sizeof(uint32_t), 1, fp);
  fread(&h->byterate, sizeof(uint32_t), 1, fp);
  fread(&h->block_align, sizeof(uint16_t), 1, fp);
  fread(&h->bits_per_sample, sizeof(uint16_t), 1, fp);
  fread(h->data_chunk_header, sizeof(uint8_t), 4, fp);
  fread(&h->data_size, sizeof(uint32_t), 1, fp);

//  printf("%ld\n", sizeof(uint32_t));

  wave->duration = h->data_size / (h->sample_rate * h->channels * (h->bits_per_sample / 8));
  wave->length = h->data_size;


  printf("(Waves) => format: %d\n", h->format_type);
  printf("(Waves) => channels: %d\n", h->channels);
  printf("(Waves) => sample_rate: %d\n", h->sample_rate);
  printf("(Waves) => byterate: %d\n", h->byterate);
  printf("(Waves) => block_align: %d\n", h->block_align);
  printf("(Waves) => bits_per_sample: %d\n", h->bits_per_sample);
  printf("(Waves) => data_size: %d\n", h->data_size);

  wave->data = (char*)calloc(wave->length, sizeof(char));
  fread(wave->data, sizeof(char), wave->length, fp);

  fclose(fp);

  return 1;
}
