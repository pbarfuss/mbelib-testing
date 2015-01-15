#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include "mbelib.h"

typedef struct
{
  unsigned char *mbe_in_data;
  unsigned int mbe_in_pos, mbe_in_size;
  unsigned char is_imbe;
  float audio_out_temp_buf[960];
  int errs;
  int errs2;
  char err_str[64];
  mbe_parms cur_mp;
  mbe_parms prev_mp;
  mbe_parms prev_mp_enhanced;
  float aout_gain;
  float aout_max_buf[33];
  unsigned int aout_max_buf_idx;
} dsd_state;

static int readAmbe2450Data (dsd_state *state, char *ambe_d)
{
  int i, j, k;
  unsigned char b;

  state->errs2 = state->mbe_in_data[state->mbe_in_pos++];
  state->errs = state->errs2;

  k = 0;
  for (i = 0; i < 6; i++) {
      b = state->mbe_in_data[state->mbe_in_pos++];
      if (state->mbe_in_pos >= state->mbe_in_size) {
          return (1);
      }
      for (j = 0; j < 8; j++) {
          ambe_d[k] = (b & 128) >> 7;
          b = b << 1;
          b = b & 255;
          k++;
      }
  }
  b = state->mbe_in_data[state->mbe_in_pos++];
  ambe_d[48] = (b & 1);

  return (0);
}

static int readImbe4400Data (dsd_state *state, char *imbe_d)
{
  int i, j, k;
  unsigned char b;

  state->errs2 = state->mbe_in_data[state->mbe_in_pos++];
  state->errs = state->errs2;

  k = 0;
  for (i = 0; i < 11; i++) {
      b = state->mbe_in_data[state->mbe_in_pos++];
      if (state->mbe_in_pos >= state->mbe_in_size) {
          return (1);
      }
      for (j = 0; j < 8; j++) {
          imbe_d[k] = (b & 128) >> 7;
          b = b << 1;
          b = b & 255;
          k++;
      }
  }
  return (0);
}

static void openMbeInFile (const char *mbe_in_file, dsd_state *state)
{
  struct stat st;
  char cookie[5];
  int mbe_in_fd = -1;

  mbe_in_fd = open (mbe_in_file, O_RDONLY);
  if (mbe_in_fd < 0) {
      printf ("Error: could not open %s\n", mbe_in_file);
  }
  fstat(mbe_in_fd, &st);
  state->mbe_in_pos = 4;
  state->mbe_in_size = st.st_size;
  state->mbe_in_data = malloc(st.st_size+1);
  read(mbe_in_fd, state->mbe_in_data, state->mbe_in_size);
  close(mbe_in_fd);

  memcpy(cookie, state->mbe_in_data, 4);
  if (!memcmp(cookie, ".amb", 4)) {
    state->is_imbe = 0;
  } else if (!memcmp(cookie, ".imb", 4)) {
    state->is_imbe = 1;
  } else {
      printf ("Error - unrecognized file type\n");
  }
}

static inline float av_clipf(float a, float amin, float amax)
{
    if      (a < amin) return amin;
    else if (a > amax) return amax;
    else               return a;
}

void
ProcessAGC(dsd_state * state)
{
  int i, n;
  float aout_abs, max, gainfactor, gaindelta, maxbuf;

  // detect max level
  max = 0;
  for (n = 0; n < 160; n++) {
      aout_abs = fabsf (state->audio_out_temp_buf[n]);
      if (aout_abs > max)
          max = aout_abs;
  }
  state->aout_max_buf[state->aout_max_buf_idx++] = max;
  if (state->aout_max_buf_idx > 24) {
      state->aout_max_buf_idx = 0;
  }

  // lookup max history
  for (i = 0; i < 25; i++) {
      maxbuf = state->aout_max_buf[i];
      if (maxbuf > max)
          max = maxbuf;
  }

  // determine optimal gain level
  if (max > 0.0f) {
      //gainfactor = (30000.0f / max);
      gainfactor = (32767.0f / max);
  } else {
      gainfactor = 50.0f;
  }
  if (gainfactor < state->aout_gain) {
      state->aout_gain = gainfactor;
      gaindelta = 0.0f;
  } else {
      if (gainfactor > 50.0f) {
          gainfactor = 50.0f;
      }
      gaindelta = gainfactor - state->aout_gain;
      if (gaindelta > (0.05f * state->aout_gain)) {
          gaindelta = (0.05f * state->aout_gain);
      }
  }

  // adjust output gain
  state->aout_gain += gaindelta;
}

static void writeSynthesizedVoice (int wav_out_fd, dsd_state *state)
{
  short aout_buf[160];
  unsigned int n;

  ProcessAGC(state);
  for (n = 0; n < 160; n++) {
    float tmp = state->audio_out_temp_buf[n];

    tmp *= state->aout_gain;
    //tmp = av_clipf(tmp, -1.0f, 1.0f);

    if (tmp > 32767.0f) {
        tmp = 32767.0f;
    } else if (tmp < -32767.0f) {
        tmp = -32767.0f;
    }
    aout_buf[n] = lrintf(tmp);
  }

  write(wav_out_fd, aout_buf, 160 * sizeof(int16_t));
}

static const unsigned char static_hdr_portion[20] = {
   0x52, 0x49, 0x46, 0x46, 0xFF, 0xFF, 0xFF, 0x7F,
   0x57, 0x41, 0x56, 0x45, 0x66, 0x6D, 0x74, 0x20,
   0x10, 0x00, 0x00, 0x00
};

typedef struct _WAVHeader {
    uint16_t wav_id;
    uint16_t channels;
    uint32_t samplerate;
    uint32_t bitrate;
    uint32_t block_align;
    uint32_t pad0;
    uint32_t pad1;
} __attribute__((packed)) WAVHeader;

static void write_wav_header(int fd, uint32_t rate)
{
   WAVHeader w;
   write(fd, &static_hdr_portion, 20);

   w.wav_id = 1;
   w.channels = 1;
   w.samplerate = rate;
   w.bitrate = rate*2;
   w.block_align = 0x00100010;
   w.pad0 = 0x61746164;
   w.pad1 = 0x7fffffff;
   write(fd, &w, sizeof(WAVHeader));
}

static void usage(void) {
    const char *usage_str = "decode_ambe: a standalone AMBE decoder for the AMBE3600x2450 format.\n"
                            "Usage: decode_ambe [AMB file] [Output Wavfile] <uvquality>\n"
                            "Where [AMB file] is an AMBE file as output by, for instance, DSD's -d option,\n"
                            "and [Output Wavfile] will be the decoded output, in signed 16-bit PCM, at 8kHz samplerate.\n"
                            "Takes an optional third argument that is the speech synthesis quality in terms of the number of waves per band.\n"
                            "Valid values lie in the closed interval [1, 64], with the default being 3.\n";
    write(1, usage_str, strlen(usage_str));
}

int main(int argc, char **argv) {
    dsd_state state;
    char ambe_d[49];
    char imbe_d[88];
    unsigned int uvquality = 3;
    int out_fd = -1;
    memset(&state, 0, sizeof(dsd_state));

    if (argc < 3) {
        usage();
        return -1;
    }

    if (argc > 3) {
        uvquality = strtoul(argv[3], NULL, 10);
    }

    out_fd = open(argv[2], O_WRONLY | O_CREAT | O_APPEND, 0644);
    write_wav_header(out_fd, 8000);

    openMbeInFile (argv[1], &state);
    state.aout_gain = 25;
    mbe_initMbeParms (&state.cur_mp, &state.prev_mp, &state.prev_mp_enhanced);
    printf ("Playing %s\n", argv[1]);
    while (state.mbe_in_pos < state.mbe_in_size) {
        char *err_str = state.err_str;
        if (state.is_imbe) {
          readImbe4400Data (&state, imbe_d);
          mbe_processImbe4400Dataf (state.audio_out_temp_buf, &state.errs2, err_str, imbe_d,
                                  &state.cur_mp, &state.prev_mp, &state.prev_mp_enhanced, uvquality);
        } else {
          readAmbe2450Data (&state, ambe_d);
          mbe_processAmbe2450Dataf (state.audio_out_temp_buf, &state.errs2, err_str, ambe_d,
                                  &state.cur_mp, &state.prev_mp, &state.prev_mp_enhanced, uvquality);
        }
        if (state.errs2 > 0) {
            printf("decodeAmbe2450Parms: errs2: %u, err_str: %s\n", state.errs2, state.err_str);
        }
        writeSynthesizedVoice (out_fd, &state);
    }
    close(out_fd);
    return 0;
}

