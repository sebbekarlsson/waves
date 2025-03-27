#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <waves/wav.h>

static float convert_to_float_from_16_bit(int16_t i) {
  float f = ((float)i) / (float)32768;
  if (f > 1)
    f = 1;
  if (f < -1)
    f = -1;

  return f;
}

static float convert_to_float_from_24_bit(int32_t i) {
  float f = ((float)i) / (float)8388608;
  if (f > 1)
    f = 1;
  if (f < -1)
    f = -1;

  return f;
}

int32_t read_24_bit_as_int32(uint8_t *data) {
  int32_t result = 0;

  result |= data[0];
  result |= data[1] << 8;
  result |= data[2] << 16;

  if (result & 0x00800000) {
    result |= 0xFF000000;
  }

  return result;
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

  //printf("nr samples: %ld\n", nr_samples);
  //printf("Original bytes: %ld\n", nr_bytes);
  //printf("Next bytes: %ld\n", next_size);

  float *next_data = (float *)calloc(nr_samples, sizeof(float));

  if (wave->header.bits_per_sample == 16) {
    int16_t *og_data = (int16_t *)wave->data;

    int16_t *end = &og_data[nr_bytes];
    int16_t *ptr = &og_data[0];
    float *fptr = &next_data[0];
    float *fend = &next_data[nr_samples];

    for (int64_t i = 0; i < nr_samples; i++) {
      int16_t *bytes = &og_data[i];
      float v = convert_to_float_from_16_bit(*bytes);
      next_data[i] = v;
    }
  } else if (wave->header.bits_per_sample == 24) {
    uint8_t *og_data = (uint8_t *)wave->data;

    for (int64_t i = 0; i < nr_samples; i++) {
      int32_t sample = read_24_bit_as_int32(&og_data[i * 3]);
      float v = convert_to_float_from_24_bit(sample);
      next_data[i] = v;
    }
  }

  free(wave->data);
  wave->data = next_data;

  int64_t next_bits_per_sample = (sizeof(float) * 8);
  wave->header.format_type = 3;
  wave->header.byterate = wave->header.sample_rate * channels * sizeof(float);
  wave->header.bits_per_sample = next_bits_per_sample;
  wave->header.length_of_fmt = 16;
  wave->header.channels = channels;
  wave->header.block_align =
      (wave->header.bits_per_sample / 8) * wave->header.channels;
  wave->header.byterate = wave->header.sample_rate * wave->header.block_align;
  wave->header.data_size = next_size;
  wave->header.overall_size = 36 + wave->header.data_size;

  wave->length = next_size;
  wave->num_samples = nr_samples;

  return 1;
}

int wav_write(Wave wave, const char *path) {
  WavHeader header = {};

  FILE *out = fopen(path, "wb");
  if (out == NULL) {
    printf("Could not write to %s\n", path);
    return 0;
  }

  const char *data_tag = "data";
  memcpy(&wave.header.data_chunk_header, data_tag, 4 * sizeof(uint8_t));

  fwrite(&wave.header, sizeof(header), 1, out);

  fseek(out, 0, SEEK_END);

  int64_t byte_size = wave.header.format_type == 3        ? sizeof(float)
                      : wave.header.bits_per_sample == 16 ? sizeof(int16_t)
                                                          : sizeof(int32_t);

  fwrite(wave.data, byte_size, wave.length / byte_size, out);

  fclose(out);

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

  fread(h->fmt_chunk_marker, sizeof(uint8_t), 4, fp);

  // skip junk if any
  while (memcmp(h->fmt_chunk_marker, "fmt ", 4) != 0) {
    // skip size of chunk
    fread(&h->length_of_fmt, sizeof(uint32_t), 1, fp);
    fseek(fp, h->length_of_fmt, SEEK_CUR);
    if ((fread(h->fmt_chunk_marker, sizeof(uint8_t), 4, fp)) == EOF) {
      printf("Error reading wav file.\n");
      fclose(fp);
      return 0;
    }
  }

  // read 'fmt ' chunk size
  fread(&h->length_of_fmt, sizeof(uint32_t), 1, fp);
  // read 'fmt ' chunk
  fread(&h->format_type, sizeof(uint16_t), 1, fp);
  fread(&h->channels, sizeof(uint16_t), 1, fp);
  fread(&h->sample_rate, sizeof(uint32_t), 1, fp);
  fread(&h->byterate, sizeof(uint32_t), 1, fp);
  fread(&h->block_align, sizeof(uint16_t), 1, fp);
  fread(&h->bits_per_sample, sizeof(uint16_t), 1, fp);

  // if 'fmt ' chunk has extra params, skip them
  if (h->length_of_fmt > 16) {
    fseek(fp, h->length_of_fmt - 16, SEEK_CUR);
  }

  // find 'data' chunk
  fread(h->data_chunk_header, sizeof(uint8_t), 4, fp);
  while (memcmp(h->data_chunk_header, "data", 4) != 0) {
    fread(&h->data_size, sizeof(uint32_t), 1, fp);
    fseek(fp, h->data_size, SEEK_CUR);
    if ((fread(h->data_chunk_header, sizeof(uint8_t), 4, fp)) == EOF) {
      printf("Error reading wav file.\n");
      fclose(fp);
      return 0;
    }
  }

  // read 'data' chunk size
  fread(&h->data_size, sizeof(uint32_t), 1, fp);

  wave->duration =
      (double)h->data_size / ((double)h->sample_rate * (double)h->channels *
                              ((double)h->bits_per_sample / 8.0f));
  wave->length = h->data_size;

  wave->data = (char *)calloc(wave->length, sizeof(char));
  fread(wave->data, sizeof(char), wave->length, fp);
  
  fclose(fp);

  wave->num_samples = wave->header.bits_per_sample <= 0 ? 0 : ((wave->length * 8) / wave->header.bits_per_sample);

  if (options.convert_to_float) {
    if ((h->format_type == 1 || h->format_type == 65534) &&
        (h->bits_per_sample == 16 || h->bits_per_sample == 24)) {
      //printf("Attempting to convert wav to float.\n");
      if (!wav_convert(wave)) {
        fprintf(stderr, "Failed to convert wav.\n");
      }
    }
  }

  return 1;
}

int wav_destroy(Wave *wave) {
  if (!wave)
    return 0;
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

void wav_print(Wave* wave, FILE* fp) {
  if (wave == 0 || fp == 0) return;
  fprintf(fp, "duration: %9.11f\n", wave->duration);
  fprintf(fp, "length_of_fmt: %d\n", wave->header.length_of_fmt);
  fprintf(fp, "length: %ld\n", wave->length);
  fprintf(fp, "data_size: %d\n", wave->header.data_size);
  fprintf(fp, "num_samples: %ld\n", wave->num_samples);
  fprintf(fp, "sample_rate: %d\n", wave->header.sample_rate);
  fprintf(fp, "channels: %d\n", wave->header.channels);
  fprintf(fp, "bits_per_sample: %d\n", wave->header.bits_per_sample);
  fprintf(fp, "byterate: %d\n", wave->header.byterate);
  fprintf(fp, "block_align: %d\n", wave->header.block_align);
  fprintf(fp, "format_type: %d\n", wave->header.format_type);
}
