#include <stdio.h>
#include <waves/wav.h>

int main(int argc, char *argv[]) {

  Wave wave = {0};
  WaveOptions options = {0};
  options.convert_to_float = true;

  if (argc < 2) {
    fprintf(stderr, "Please specify input file.\n");
    return 1;
  }

  if (!wav_read(&wave, argv[1], options)) {
    printf("Error reading wav file.\n");
    return 1;
  }


  if (!wav_write(wave, "out_float.wav")) {
    fprintf(stderr, "Error writing wav file.\n");
    return 1;
  }
  wav_print(&wave, stdout);

  return 0;
}
