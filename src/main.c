#include <stdio.h>
#include <waves/wav.h>

int main(int argc, char *argv[]) {
  uint32_t len = 0;
  WavHeader h = {};
  float *data = wav_read("assets/F-bounce-3.wav", &len, &h);

  if (data == 0) {
    printf("Error reading wav file.\n");
    return 1;
  }

  if (!wav_write("out.wav", data, len * sizeof(float), h.sample_rate,
                 h.bits_per_sample, h.channels, h.format_type)) {

    printf("Error writing wav file.\n");
    return 1;
  }

  return 0;
}
