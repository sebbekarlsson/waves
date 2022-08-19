#include <stdio.h>
#include <waves/wav.h>

int main(int argc, char *argv[]) {

  Wave wave = {0};

  if (!wav_read(&wave, "../assets/flute.wav")) {
    printf("Error reading wav file.\n");
    return 1;
  }

  if (!wav_write(wave, "out_float.wav")) {
    fprintf(stderr, "Error writing wav file.\n");
    return 1;
  }

  return 0;
}
