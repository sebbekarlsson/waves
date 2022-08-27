#ifndef WAVES_WAV_H
#define WAVES_WAV_H
#include <stdint.h>
#include <stdbool.h>

typedef struct WAV_HEADER {
  uint8_t riff[4];
  uint32_t overall_size;
  uint8_t wave[4];
  uint8_t fmt_chunk_marker[4];
  uint32_t length_of_fmt;
  uint16_t format_type;
  uint16_t channels;
  uint32_t sample_rate;
  uint32_t byterate;
  uint16_t block_align;
  uint16_t bits_per_sample;
  uint8_t data_chunk_header[4];
  uint32_t data_size;
} WavHeader;


typedef struct {
  WavHeader header;
  int64_t length;
  double duration;
  void* data;
} Wave;


typedef struct {
  bool convert_to_float;
} WaveOptions;

int wav_write(Wave wave, const char* path);

int wav_read(Wave* wave, const char *path, WaveOptions options);

int wav_destroy(Wave* wave);

#endif
