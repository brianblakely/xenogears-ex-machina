// Stereo -> 5.1 upmix filter around FreeSurround (Christian Kothe, GPL-2.0+),
// for testing the game's Wide sound mode. Test tool only: never linked into
// the runtime.
//
//     freesurround SAMPLERATE < stereo.f32 > surround.f32
//
// Input is interleaved float32 L/R; output is interleaved float32
// FL FC FR BL BR LFE, aligned with the input (the decoder's half-block lag is
// removed) and the same length.

#include <FreeSurround/FreeSurroundDecoder.h>

#include <cstdio>
#include <cstdlib>
#include <vector>

int main(int argc, char **argv) {
  if (argc != 2) {
    std::fprintf(stderr, "usage: freesurround SAMPLERATE < stereo.f32 > surround.f32\n");
    return 2;
  }
  const unsigned rate = std::strtoul(argv[1], nullptr, 10);
  const unsigned block = 4096, channels = 6;
  DPL2FSDecoder decoder;
  decoder.Init(cs_5point1, block, rate);

  std::vector<float> input;
  float buffer[4096];
  for (size_t n; (n = std::fread(buffer, sizeof(float), 4096, stdin)) > 0;)
    input.insert(input.end(), buffer, buffer + n);
  const size_t frames = input.size() / 2, lag = block / 2;
  input.resize((frames + lag + block - 1) / block * block * 2, 0.0f);

  size_t written = 0, produced = 0;
  for (size_t at = 0; at < input.size(); at += block * 2) {
    const float *out = decoder.decode(&input[at]);
    for (unsigned k = 0; k < block; k++, produced++) {
      if (produced < lag || written >= frames)
        continue;
      std::fwrite(out + k * channels, sizeof(float), channels, stdout);
      written++;
    }
  }
  return written == frames ? 0 : 1;
}
