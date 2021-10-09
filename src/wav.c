#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <waves/wav.h>

int wav_write(const char *path, float *data, uint32_t size, float sample_freq,
              uint16_t bits_per_sample, uint16_t nr_channels,
              uint16_t format_type) {
  WavHeader header = {};

  FILE *out = fopen(path, "wb");
  if (out == NULL) {
    printf("Could not write to %s\n", path);
    return 0;
  }

  uint32_t byterate = sample_freq * nr_channels * (bits_per_sample / 8);
  uint64_t blockalign = (bits_per_sample / 8) * nr_channels;

  strncpy(header.riff, "RIFF", 4);
  strncpy(header.wave, "WAVE", 4);
  strncpy(header.fmt_chunk_marker, "fmt ", 4);
  strncpy(header.data_chunk_header, "data", 4);

  header.length_of_fmt = 16;
  header.format_type = format_type;
  header.channels = nr_channels;
  header.sample_rate = sample_freq;
  header.bits_per_sample = bits_per_sample;
  header.byterate =
      header.sample_rate * header.channels * (header.bits_per_sample / 8);
  header.block_align = (header.bits_per_sample / 8) * header.channels;
  header.data_size = sizeof(&data[0]) * (size >> 3);
  header.overall_size = (header.data_size);

  printf("Writing file: %s\n", path);

  fwrite(&header, sizeof(header), 1, out);

  fseek(out, 0, SEEK_END);

  fwrite(&data[0], sizeof(&data[0]), size, out);

  fclose(out);

  printf("Done, wrote file %s\n", path);

  return 1;
}

static float convert_to_float(uint16_t i) {
  float f = ((float)i) / (float)32768;
  if (f > 1)
    f = 1;
  if (f < -1)
    f = -1;

  return f;
}

float *wav_read(const char *path, uint32_t *length, WavHeader *header) {
  if (access(path, F_OK) != 0) {
    printf("could not open `%s`\n", path);
    return 0;
  }

  FILE *f = fopen(path, "rb");

  WavHeader h = {};

  fread(h.riff, sizeof(uint8_t), 4, f);
  fread(&h.overall_size, sizeof(uint32_t), 1, f);
  fread(h.wave, sizeof(uint8_t), 4, f);

  // skip junk if any
  while (memcmp(h.fmt_chunk_marker, "fmt ", 4) != 0) {
    if ((fread(h.fmt_chunk_marker, sizeof(uint8_t), 4, f)) == EOF) {
      printf("Error reading wav file.\n");
      return 0;
      fclose(f);
    }
  }

  fread(&h.length_of_fmt, sizeof(uint32_t), 1, f);
  fread(&h.format_type, sizeof(uint16_t), 1, f);
  fread(&h.channels, sizeof(uint16_t), 1, f);
  fread(&h.sample_rate, sizeof(uint32_t), 1, f);
  fread(&h.byterate, sizeof(uint32_t), 1, f);
  fread(&h.block_align, sizeof(uint16_t), 1, f);
  fread(&h.bits_per_sample, sizeof(uint16_t), 1, f);
  fread(h.data_chunk_header, sizeof(uint8_t), 4, f);
  fread(&h.data_size, sizeof(uint32_t), 1, f);

  *header = h;

  int len = h.bits_per_sample;

  float *actual_data = 0;

  if (h.format_type == 3) {
    float *data = (float *)calloc(h.data_size, sizeof(float));
    fread(data, sizeof(float), h.data_size, f);
    *length = h.data_size >> 2;
    fclose(f);
    return data;
  } else if (h.format_type == 1) {
    printf("Wav data will be converted to float.\n");
    unsigned char *data =
        (unsigned char *)calloc(h.data_size, sizeof(unsigned char));
    fread(data, sizeof(unsigned char), h.data_size, f);
    *length = h.data_size >> 2;
    fclose(f);

    actual_data = (float *)calloc(h.data_size, sizeof(float));

    for (int i = 0; i < h.data_size; i++) {
      actual_data[i] = convert_to_float(data[i]);
    }

    return actual_data;
  } else {
    printf("Wav: unsupported format type.\n");
  }

  printf("Wav error.\n");
  return 0;
}
