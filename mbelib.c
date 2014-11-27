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

/**
 * \return A pseudo-random float between [0.0, 1.0].
 * See http://www.azillionmonkeys.com/qed/random.html for further improvements
 */
static float
mbe_rand()
{
  float f = (float)((uint16_t)lfg_rand() * 1.52587890625e-5f);
  return f;
}

/**
 * \return A pseudo-random float between [-pi, +pi].
 */
static float
mbe_rand_phase()
{
  return mbe_rand() * (((float)M_PI) * 2.0F) - ((float)M_PI);
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

  prev_mp->w0 = 0.09378f;
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

void mbe_spectralAmpEnhance (mbe_parms * cur_mp)
{
  float tmp, sum, gamma, M, Rm0, Rm1, R2m0, R2m1, Wl[57];
  int l;

  Rm0 = 0;
  Rm1 = 0;
  for (l = 1; l <= cur_mp->L; l++) {
      tmp = cur_mp->Ml[l];
      tmp *= tmp;
      Rm0 = Rm0 + tmp;
      Rm1 = Rm1 + (tmp * mbe_cosf (cur_mp->w0 * (float) l)); // mbe_cosf(x) bounded by M_PI at least for AMBE
  }
  R2m0 = (Rm0 * Rm0);
  R2m1 = (Rm1 * Rm1);

  for (l = 1; l <= cur_mp->L; l++) {
      if (cur_mp->Ml[l] != 0) {
          // mbe_cosf(x) here bounded similarly
          tmp = mbe_sqrtf(mbe_sqrtf(((0.96f * M_PI * ((R2m0 + R2m1) - (2.0f * Rm0 * Rm1 * mbe_cosf (cur_mp->w0 * (float) l)))) / (cur_mp->w0 * Rm0 * (R2m0 - R2m1)))));
          Wl[l] = mbe_sqrtf (cur_mp->Ml[l]) * tmp; 

          if ((8 * l) <= cur_mp->L) {
          } else if (Wl[l] > 1.2f) {
              cur_mp->Ml[l] = 1.2f * cur_mp->Ml[l];
          } else if (Wl[l] < 0.5f) {
              cur_mp->Ml[l] = 0.5f * cur_mp->Ml[l];
          } else {
              cur_mp->Ml[l] = Wl[l] * cur_mp->Ml[l];
          }
      }
  }

  // generate scaling factor
  sum = 0;
  for (l = 1; l <= cur_mp->L; l++) {
      M = cur_mp->Ml[l];
      if (M < 0) {
          M = -M;
      }
      sum += (M * M);
  }
  if (sum == 0) {
      gamma = 1.0f;
  } else {
      gamma = mbe_sqrtf (Rm0 / sum);
  }

  // apply scaling factor
  for (l = 1; l <= cur_mp->L; l++) {
      cur_mp->Ml[l] = gamma * cur_mp->Ml[l];
  }
}

const float AmbeLog2f[128] = {
    0.000000000, 0.000000000, 1.000000000, 1.584962487, 2.000000000, 2.321928024, 2.584962606, 2.807354927,
    3.000000000, 3.169924974, 3.321928024, 3.459431648, 3.584962368, 3.700439453, 3.807354927, 3.906890631,
    4.000000000, 4.087462902, 4.169925213, 4.247927189, 4.321928024, 4.392317295, 4.459431648, 4.523561954,
    4.584962368, 4.643856049, 4.700439453, 4.754887581, 4.807354927, 4.857980728, 4.906890392, 4.954195976,
    5.000000000, 5.044394016, 5.087462902, 5.129282951, 5.169925213, 5.209453106, 5.247927189, 5.285401821,
    5.321928024, 5.357552052, 5.392317295, 5.426264763, 5.459431648, 5.491853237, 5.523561954, 5.554588795,
    5.584962368, 5.614709854, 5.643856049, 5.672425270, 5.700439453, 5.727920055, 5.754887581, 5.781359673,
    5.807354450, 5.832890034, 5.857980728, 5.882643223, 5.906890392, 5.930737019, 5.954195976, 5.977279663,
    6.000000000, 6.022367954, 6.044394016, 6.066089630, 6.087462902, 6.108524323, 6.129282951, 6.149747372,
    6.169925213, 6.189824581, 6.209453583, 6.228818893, 6.247927189, 6.266786098, 6.285402298, 6.303780556,
    6.321928024, 6.339849949, 6.357552052, 6.375039101, 6.392317295, 6.409390926, 6.426264286, 6.442943096,
    6.459431648, 6.475733757, 6.491853237, 6.507794380, 6.523561954, 6.539158821, 6.554588795, 6.569855690,
    6.584962368, 6.599912643, 6.614709854, 6.629356861, 6.643856049, 6.658211231, 6.672425270, 6.686500549,
    6.700439930, 6.714245319, 6.727920532, 6.741466522, 6.754887581, 6.768184662, 6.781359673, 6.794415951,
    6.807354450, 6.820178986, 6.832890034, 6.845489979, 6.857980728, 6.870365143, 6.882643223, 6.894817352,
    6.906890392, 6.918863297, 6.930737019, 6.942514420, 6.954195976, 6.965784550, 6.977279663, 6.988684654
};


void mbe_synthesizeSilencef (float *aout_buf)
{
  float *aout_buf_p = aout_buf;
  unsigned int n;

  for (n = 0; n < 160; n++) {
      *aout_buf_p = 0.0f;
      aout_buf_p++;
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
  uvthreshold = ((uvthresholdf * M_PI) / 4000.0f);

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
      cur_mp->PSIl[l] = prev_mp->PSIl[l] + ((pw0 + cw0) * ((float) (l * N) * 0.5f));
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
              C1 = Ws[n + N] * prev_mp->Ml[l] * mbe_cosf ((pw0l * (float) n) + prev_mp->PHIl[l]);
              C3 = 0;
              // unvoiced multisine mix
              for (i = 0; i < uvquality; i++) {
                  C3 = C3 + mbe_cosf ((cw0 * (float) n * ((float) l + ((float) i * uvstep) - uvoffset)) + rphase[i]);
                  if (cw0l > uvthreshold) {
                      C3 = C3 + ((cw0l - uvthreshold) * uvrand * mbe_rand());
                  }
              }
              C3 = C3 * uvsine * Ws[n] * cur_mp->Ml[l] * qfactor;
              *Ss = *Ss + C1 + C3;
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
              C1 = Ws[n] * cur_mp->Ml[l] * mbe_cosf ((cw0l * (float) (n - N)) + cur_mp->PHIl[l]);
              C3 = 0;
              // unvoiced multisine mix
              for (i = 0; i < uvquality; i++) {
                  C3 = C3 + mbe_cosf ((pw0 * (float) n * ((float) l + ((float) i * uvstep) - uvoffset)) + rphase[i]);
                  if (pw0l > uvthreshold) {
                      C3 = C3 + ((pw0l - uvthreshold) * uvrand * mbe_rand());
                  }
              }
              C3 = C3 * uvsine * Ws[n + N] * prev_mp->Ml[l] * qfactor;
              *Ss = *Ss + C1 + C3;
              Ss++;
          }
//      } else if (((cur_mp->Vl[l] == 1) || (prev_mp->Vl[l] == 1)) && ((l >= 8) || (fabsf (cw0 - pw0) >= ((float) 0.1 * cw0)))) {
      } else if ((cur_mp->Vl[l] == 1) || (prev_mp->Vl[l] == 1)) {
          Ss = aout_buf;
          for (n = 0; n < N; n++) { 
              C1 = 0;
              // eq 133-1
              C1 = Ws[n + N] * prev_mp->Ml[l] * mbe_cosf ((pw0l * (float) n) + prev_mp->PHIl[l]);
              C2 = 0;
              // eq 133-2
              C2 = Ws[n] * cur_mp->Ml[l] * mbe_cosf ((cw0l * (float) (n - N)) + cur_mp->PHIl[l]);
              *Ss = *Ss + C1 + C2;
              Ss++;
          }
      }
/*
      // expensive and unnecessary?
      else if ((cur_mp->Vl[l] == 1) || (prev_mp->Vl[l] == 1))
        {
          Ss = aout_buf;
          // eq 137
          deltaphil = cur_mp->PHIl[l] - prev_mp->PHIl[l] - (((pw0 + cw0) * (float) (l * N)) / (float) 2);
          // eq 138
          deltawl = ((float) 1 / (float) N) * (deltaphil - ((float) 2 * M_PI * (int) ((deltaphil + M_PI) / (M_PI * (float) 2))));
          for (n = 0; n < N; n++)
            {
              // eq 136
              thetaln = prev_mp->PHIl[l] + ((pw0l + deltawl) * (float) n) + (((cw0 - pw0) * ((float) (l * n * n)) / (float) (2 * N)));
              // eq 135
              aln = prev_mp->Ml[l] + (((float) n / (float) N) * (cur_mp->Ml[l] - prev_mp->Ml[l]));
              // eq 134
              *Ss = *Ss + (aln * mbe_cosf (thetaln));
              Ss++;
            }
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
                  C3 = C3 + mbe_cosf ((pw0 * (float) n * ((float) l + ((float) i * uvstep) - uvoffset)) + rphase[i]);
                  if (pw0l > uvthreshold) {
                      C3 = C3 + ((pw0l - uvthreshold) * uvrand * mbe_rand());
                  }
              }
              C3 = C3 * uvsine * Ws[n + N] * prev_mp->Ml[l] * qfactor;
              C4 = 0;
              // unvoiced multisine mix
              for (i = 0; i < uvquality; i++) {
                  C4 = C4 + mbe_cosf ((cw0 * (float) n * ((float) l + ((float) i * uvstep) - uvoffset)) + rphase2[i]);
                  if (cw0l > uvthreshold) {
                      C4 = C4 + ((cw0l - uvthreshold) * uvrand * mbe_rand());
                  }
              }
              C4 = C4 * uvsine * Ws[n] * cur_mp->Ml[l] * qfactor;
              *Ss = *Ss + C3 + C4;
              Ss++;
          }
      }
  }
  return 0;
}

