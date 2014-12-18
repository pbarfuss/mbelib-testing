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
  float audio_out_temp_buf[960];
  int errs;
  int errs2;
  char err_str[64];
  mbe_parms cur_mp;
  mbe_parms prev_mp;
  mbe_parms prev_mp_enhanced;
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
  if (memcmp(cookie, ".amb", 4)) {
      printf ("Error - unrecognized file type\n");
  }
}

static inline float av_clipf(float a, float amin, float amax)
{
    if      (a < amin) return amin;
    else if (a > amax) return amax;
    else               return a;
}

static void writeSynthesizedVoice (int wav_out_fd, dsd_state *state)
{
  short aout_buf[160];
  unsigned int n;

  for (n = 0; n < 160; n++) {
    float tmp = state->audio_out_temp_buf[n];

    //update_volume(&s, fabsf(tmp));
    //tmp = av_clipf(tmp * get_volume(&s), -1.0f, 1.0f);

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
    mbe_initMbeParms (&state.cur_mp, &state.prev_mp, &state.prev_mp_enhanced);
    printf ("Playing %s\n", argv[1]);
    while (state.mbe_in_pos < state.mbe_in_size) {
        unsigned int i, bad;
        char *err_str = state.err_str;
        readAmbe2450Data (&state, ambe_d);
        for (i = 0; i < state.errs2; i++) {
            *err_str++ = '=';
        }
        bad = mbe_decodeAmbe2450Parms (ambe_d, &state.cur_mp, &state.prev_mp);
        if (bad) {
            printf("decodeAmbe2400Parms: bad = %u\n", bad);
        }
        if (!bad) {
            mbe_processAmbe2450Dataf (state.audio_out_temp_buf, &state.errs2, err_str, ambe_d,
                                      &state.cur_mp, &state.prev_mp, &state.prev_mp_enhanced, uvquality);
        }
        if (bad) {
            printf("decodeAmbe2400Parms: errs: %u, errs2: %u, err_str: %s\n", state.errs, state.errs2, state.err_str);
        }
        writeSynthesizedVoice (out_fd, &state);
    }
    close(out_fd);
    return 0;
}

