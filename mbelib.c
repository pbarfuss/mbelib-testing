/*
 * Copyright (C) 2010 mbelib Author
 * GPG Key ID: 0xEA5EFE2C (9E7A 5527 9CDC EBF7 BF1B  D772 4F98 E863 EA5E FE2C)
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <sys/time.h>
#include "mbelib.h"
#include "mbelib_const.h"
static AVLFG c;

typedef union _f32 {
    float f;
    uint32_t i;
} _f32;

#if 0
static void lfg_srand(unsigned int seed){
    uint32_t i, tmp[4]={0};

    for(i=0; i<64; i+=4){
        tmp[0]=seed; tmp[3]=i;
        MD5((uint8_t*)tmp, (const uint8_t*)tmp, 16);
        c.state[i  ]= tmp[0];
        c.state[i+1]= tmp[1];
        c.state[i+2]= tmp[2];
        c.state[i+3]= tmp[3];
    }
    c.index=0;
}
#endif
static __attribute__((always_inline)) inline unsigned int lcg_rand(unsigned int previous_val)
{
    return previous_val * 1664525 + 1013904223;
}

static void lfg_srand(AVLFG *c, unsigned int seed){
    uint32_t i, tmp = seed;

    for(i=0; i<64; i+=1){
        tmp = lcg_rand(tmp);
        c->state[i]= tmp;
    }
    c->index=0;
}

static unsigned int lfg_rand(void){
    c.state[c.index & 63] = c.state[(c.index-24) & 63] + c.state[(c.index-55) & 63];
    return c.state[c.index++ & 63];
}

#if 0
uint32_t
next_u(uint32_t u) {
    return (u * 171 + 11213) % 53125;
}
#endif

/**
 * \return A pseudo-random float between [0.0, 1.0].
 * See http://www.azillionmonkeys.com/qed/random.html for further improvements
 */
static inline float
mbe_rand()
{
  return (float)((uint16_t)lfg_rand() * 1.52587890625e-5f);
}

/**
 * \return A pseudo-random float between [-pi, +pi].
 */
static float
mbe_rand_phase()
{
  float f = (float)((int16_t)lfg_rand() * 3.051757e-5f);
  return ((float)M_PI * f);
  //return mbe_rand() * (((float)M_PI) * 2.0F) - ((float)M_PI);
}

void mbe_printVersion (char *str)
{
  strcpy(str, MBELIB_VERSION);
}

void mbe_initMbeParms (mbe_parms * cur_mp, mbe_parms * prev_mp, mbe_parms * prev_mp_enhanced)
{
  struct timeval tv;
  int l;

  gettimeofday(&tv, NULL);
  lfg_srand(&c, tv.tv_sec);

  prev_mp->w0 = 0.0298511f;
  prev_mp->L = 30;
  prev_mp->K = 10;
  prev_mp->gamma = (float) 0;
  for (l = 0; l <= 56; l++)
    {
      prev_mp->Ml[l] = (float) 0;
      prev_mp->Vl[l] = 0;
      prev_mp->log2Ml[l] = (float) 0;   // log2 of 1 == 0
      prev_mp->PHIl[l] = (float) 0;
      prev_mp->PSIl[l] = (M_PI * 0.5f);
    }
  prev_mp->repeat = 0;
  mbe_moveMbeParms (prev_mp, cur_mp);
  mbe_moveMbeParms (prev_mp, prev_mp_enhanced);
}

void mbe_synthesizeSilencef (float *aout_buf)
{
  unsigned int n;

  for (n = 0; n < 160; n++) {
      aout_buf[n] = 0.0f;
  }
}

int mbe_synthesizeSpeechf (float *aout_buf, mbe_parms * cur_mp, mbe_parms * prev_mp, int uvquality)
{
  int i, l, n, maxl;
  float *Ss, loguvquality;
  float C1, C2, C3, C4;
  float deltaphil, deltawl, thetaln, aln;
  int numUv;
  float cw0, pw0, cw0l, pw0l;
  float uvsine, uvrand, uvthreshold, uvthresholdf;
  float uvstep, uvoffset;
  float qfactor;
  float rphase[64], rphase2[64];
  const int N = 160;

  uvthresholdf = 2700.0f;
  uvthreshold = (uvthresholdf / 4000.0f);

  // voiced/unvoiced/gain settings
  uvsine = 1.3591409f *M_E;
  uvrand = 2.0f;

  if ((uvquality < 1) || (uvquality > 64)) {
#ifdef MBE_DEBUG
      printf ("\nmbelib:  Error - uvquality must be within the range 1 - 64\n");
#endif
      return -1;
  }

  // calculate loguvquality
  if (uvquality == 1) {
      loguvquality = (float) 1 / M_E;
  } else {
      loguvquality = AmbeLog2f[uvquality] / ((float) uvquality * (float)M_LOG2E);
  }

  // calculate unvoiced step and offset values
  uvstep = 1.0f / (float) uvquality;
  qfactor = loguvquality;
  uvoffset = (uvstep * (float) (uvquality - 1)) * 0.5f;

  // count number of unvoiced bands
  numUv = 0;
  for (l = 1; l <= cur_mp->L; l++) {
      if (cur_mp->Vl[l] == 0) {
          numUv++;
      }
  }
  cw0 = cur_mp->w0;
  pw0 = prev_mp->w0;

  // init aout_buf
  Ss = aout_buf;
  for (n = 0; n < N; n++) {
      *Ss = 0.0f;
      Ss++;
  }

  // eq 128 and 129
  if (cur_mp->L > prev_mp->L) {
      maxl = cur_mp->L;
      for (l = prev_mp->L + 1; l <= maxl; l++) {
          prev_mp->Ml[l] = 0.0f;
          prev_mp->Vl[l] = 1;
      }
  } else {
      maxl = prev_mp->L;
      for (l = cur_mp->L + 1; l <= maxl; l++) {
          cur_mp->Ml[l] = 0.0f;
          cur_mp->Vl[l] = 1;
      }
  }

  // update phil from eq 139,140
  for (l = 1; l <= 56; l++) {
      cur_mp->PSIl[l] = prev_mp->PSIl[l] + ((float)M_PI * (pw0 + cw0) * ((float) (l * N) * 0.5f));
      if (l <= (int) (cur_mp->L / 4)) {
          cur_mp->PHIl[l] = cur_mp->PSIl[l];
      } else {
          cur_mp->PHIl[l] = cur_mp->PSIl[l] + ((numUv * mbe_rand_phase()) / cur_mp->L);
      }
  }

  for (l = 1; l <= maxl; l++) {
      cw0l = (cw0 * (float) l);
      pw0l = (pw0 * (float) l);
      if ((cur_mp->Vl[l] == 0) && (prev_mp->Vl[l] == 1)) {
          Ss = aout_buf;
          // init random phase
          for (i = 0; i < uvquality; i++) {
              rphase[i] = mbe_rand_phase();
          }
          for (n = 0; n < N; n++) {
              C1 = 0;
              // eq 131
              C1 = Ws[n + N] * prev_mp->Ml[l] * mbe_cosf (((float)M_PI * pw0l * (float) n) + prev_mp->PHIl[l]);
              C3 = 0;
              // unvoiced multisine mix
              for (i = 0; i < uvquality; i++) {
                  // float tmp;
                  // tmp = (mbe_cosf((float)M_PI * cw0 * (float) n * ((float) l + ((float) i * uvstep) - uvoffset)) *
                  //        mbe_cosf(rphase[i])) +
                  //       (mbe_cosf((float)M_PI * (0.5f - (cw0 * (float) n * ((float) l + ((float) i * uvstep) - uvoffset))) *
                  //        mbe_sinf(rphase[i]));
                  C3 = C3 + mbe_cosf (((float)M_PI * cw0 * (float) n * ((float) l + ((float) i * uvstep) - uvoffset)) + rphase[i]);
                  if (cw0l > uvthreshold) {
                      C3 = C3 + ((float)M_PI * (cw0l - uvthreshold) * uvrand * mbe_rand());
                  }
              }
              C3 = C3 * uvsine * Ws[n] * cur_mp->Ml[l] * qfactor;
              *Ss += C1 + C3;
              Ss++;
          }
      } else if ((cur_mp->Vl[l] == 1) && (prev_mp->Vl[l] == 0)) {
          Ss = aout_buf;
          // init random phase
          for (i = 0; i < uvquality; i++) {
              rphase[i] = mbe_rand_phase();
          }
          for (n = 0; n < N; n++) {
              C1 = 0;
              // eq 132
              C1 = Ws[n] * cur_mp->Ml[l] * mbe_cosf (((float)M_PI * cw0l * (float) (n - N)) + cur_mp->PHIl[l]);
              C3 = 0;
              // unvoiced multisine mix
              for (i = 0; i < uvquality; i++) {
                  C3 = C3 + mbe_cosf (((float)M_PI * pw0 * (float) n * ((float) l + ((float) i * uvstep) - uvoffset)) + rphase[i]);
                  if (pw0l > uvthreshold) {
                      C3 = C3 + ((float)M_PI * (pw0l - uvthreshold) * uvrand * mbe_rand());
                  }
              }
              C3 = C3 * uvsine * Ws[n + N] * prev_mp->Ml[l] * qfactor;
              *Ss += C1 + C3;
              Ss++;
          }
//      } else if (((cur_mp->Vl[l] == 1) || (prev_mp->Vl[l] == 1)) && ((l >= 8) || (fabsf (cw0 - pw0) >= ((float) 0.1 * cw0)))) {
      } else if ((cur_mp->Vl[l] == 1) || (prev_mp->Vl[l] == 1)) {
          Ss = aout_buf;
          for (n = 0; n < N; n++) { 
              C1 = 0;
              // eq 133-1
              C1 = Ws[n + N] * prev_mp->Ml[l] * mbe_cosf (((float)M_PI * pw0l * (float) n) + prev_mp->PHIl[l]);
              C2 = 0;
              // eq 133-2
              C2 = Ws[n] * cur_mp->Ml[l] * mbe_cosf (((float)M_PI * cw0l * (float) (n - N)) + cur_mp->PHIl[l]);
              *Ss += C1 + C2;
              Ss++;
          }
      }
/*
      // expensive and unnecessary?
      else if ((cur_mp->Vl[l] == 1) || (prev_mp->Vl[l] == 1))
        {
          // eq 137
          // eq 138
          // eq 136
          // eq 135
          // eq 134
        }
*/
      else {
          Ss = aout_buf;
          // init random phase
          for (i = 0; i < uvquality; i++) {
              rphase[i] = mbe_rand_phase();
          }
          // init random phase
          for (i = 0; i < uvquality; i++) {
              rphase2[i] = mbe_rand_phase();
          }
          for (n = 0; n < N; n++) {
              C3 = 0;
              // unvoiced multisine mix
              for (i = 0; i < uvquality; i++) {
                  C3 = C3 + mbe_cosf (((float)M_PI * pw0 * (float) n * ((float) l + ((float) i * uvstep) - uvoffset)) + rphase[i]);
                  if (pw0l > uvthreshold) {
                      C3 = C3 + ((float)M_PI * (pw0l - uvthreshold) * uvrand * mbe_rand());
                  }
              }
              C3 = C3 * uvsine * Ws[n + N] * prev_mp->Ml[l] * qfactor;
              C4 = 0;
              // unvoiced multisine mix
              for (i = 0; i < uvquality; i++) {
                  C4 = C4 + mbe_cosf (((float)M_PI * cw0 * (float) n * ((float) l + ((float) i * uvstep) - uvoffset)) + rphase2[i]);
                  if (cw0l > uvthreshold) {
                      C4 = C4 + ((float)M_PI * (cw0l - uvthreshold) * uvrand * mbe_rand());
                  }
              }
              C4 = C4 * uvsine * Ws[n] * cur_mp->Ml[l] * qfactor;
              *Ss += C3 + C4;
              Ss++;
          }
      }
  }
  return 0;
}

