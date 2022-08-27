#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <waves/wav.h>

int wav_write(Wave wave, const char *path) {
  WavHeader header = {};

  FILE *out = fopen(path, "wb");
  if (out == NULL) {
    printf("Could not write to %s\n", path);
    return 0;
  }

  const char* data_tag = "data";
  memcpy(&wave.header.data_chunk_header, data_tag, 4*sizeof(uint8_t));

  printf("Writing file: %s\n", path);

  fwrite(&wave.header, sizeof(header), 1, out);

  fseek(out, 0, SEEK_END);

  int64_t byte_size =
      wave.header.format_type == 3 ? sizeof(float) : sizeof(unsigned char);//sizeof(&wave.data[0]);

  fwrite(wave.data, byte_size, wave.length, out);

  fclose(out);

  printf("Done, wrote file %s\n", path);

  return 1;
}

typedef int16_t OgType;

static float convert_to_float(OgType i) {
  float f = ((float)i) / (float)32768;
  if (f > 1)
    f = 1;
  if (f < -1)
    f = -1;

  return f;
}

float convert(const OgType *src) {
  int i = src[2] << 24 | src[1] << 16 | src[0] << 8;
  return (((float)i) / (float)(INT_MAX - 256));
}

float convert2(const OgType *src) {
  int i = ((src[2] << 24) | (src[1] << 16) | (src[0] << 8)) >> 8;
  return (((float)i)) / 8388607.0;
}

float convert3(const OgType *src) {
  OgType v = *src;
  return convert_to_float(v);
}

int wav_convert(Wave *wave) {
  if (!wave->data)
    return 0;

  int64_t channels = wave->header.channels;
  int64_t nr_bytes = wave->length;
  int64_t nr_bits = nr_bytes * 8;
  int64_t bits_per_sample = wave->header.bits_per_sample;
  int64_t nr_samples = nr_bits / bits_per_sample;

  int64_t next_size = nr_samples * sizeof(float);

  printf("nr samples: %ld\n", nr_samples);
  printf("Original bytes: %ld\n", nr_bytes);
  printf("Next bytes: %ld\n", next_size);

  float *next_data = (float *)calloc(nr_samples, sizeof(float));

  OgType *og_data = (OgType *)wave->data;

  OgType *end = &og_data[nr_bytes];
  OgType *ptr = &og_data[0];
  float *fptr = &next_data[0];
  float *fend = &next_data[nr_samples];

  for (int64_t i = 0; i < nr_samples; i++) {
    OgType *bytes = &og_data[i];
    float v = convert3(bytes);
    next_data[i] = v;
  }

  free(wave->data);
  wave->data = next_data;

  int64_t next_bits_per_sample = (sizeof(float) * 8);

  wave->header.format_type = 3;
  wave->header.byterate = wave->header.sample_rate * channels * sizeof(float);
  wave->header.bits_per_sample = next_bits_per_sample;
  wave->header.data_size = next_size;
  wave->header.overall_size = sizeof(wave->header) + next_size;
  wave->header.block_align = channels * next_bits_per_sample / 8;
  wave->length = next_size;

  return 1;
}

int wav_read(Wave *wave, const char *path, WaveOptions options) {

  if (access(path, F_OK) != 0) {
  failed_to_read:
    printf("could not open `%s`\n", path);
    return 0;
  }

  FILE *fp = fopen(path, "rb");

  if (!fp) {
    goto failed_to_read;
  }

  WavHeader *h = &wave->header;

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
  fread(&h->data_chunk_header[0], sizeof(uint8_t), 4, fp);
  fread(&h->data_size, sizeof(uint32_t), 1, fp);


  if (memcmp(h->data_chunk_header, "LIST", 4) == 0) {
    fseek(fp, h->data_size, SEEK_CUR);
    fread(&h->data_size, sizeof(uint32_t), 1, fp);
  }



  //  printf("%ld\n", sizeof(uint32_t));

  /*
  printf("(Waves) => format: %d\n", h->format_type);
  printf("(Waves) => channels: %d\n", h->channels);
  printf("(Waves) => sample_rate: %d\n", h->sample_rate);
  printf("(Waves) => byterate: %d\n", h->byterate);
  printf("(Waves) => block_align: %d\n", h->block_align);
  printf("(Waves) => bits_per_sample: %d\n", h->bits_per_sample);
  printf("(Waves) => data_size: %d\n", h->data_size);
  printf("(Waves) => duration: %12.6f\n", wave->duration);*/

  wave->data = (char *)calloc(wave->length, sizeof(char));
  fread(wave->data, sizeof(char), wave->length, fp);

  fclose(fp);

  if (options.convert_to_float) {
    if (h->format_type == 1 && h->bits_per_sample == 16) {
      printf("Attempting to convert wav to float.\n");
      if (!wav_convert(wave)) {
        fprintf(stderr, "Failed to convert wav.\n");
      }
    }
  }

  wave->duration =
      (float)h->data_size / ((float)h->sample_rate * (float)h->channels * ((float)h->bits_per_sample / 8.0f));
  wave->length = h->data_size;

  return 1;
}

int wav_destroy(Wave* wave) {
  if (!wave) return 0;
  if (wave->data) {
    free(wave->data);
    wave->data = 0;
  }

  wave->length = 0;
  wave->duration = 0;
  wave->data = 0;
  wave->header.overall_size = 0;
  wave->header.data_size = 0;
  wave->header.sample_rate = 0;
  wave->header.length_of_fmt = 0;
  wave->header.bits_per_sample = 0;
  wave->header.block_align = 0;
  wave->header.byterate = 0;
  wave->header.channels = 0;
  wave->header.length_of_fmt = 0;

  return 1;
}
