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

#ifndef _MBELIB_H
#define _MBELIB_H

#define MBELIB_VERSION "1.2.5"

typedef struct mbe_parameters
{
  float w0;
  int L;
  int K;
  int Vl[57];
  float Ml[57];
  float log2Ml[57];
  float PHIl[57];
  float PSIl[57];
  float gamma;
  int un;
  int repeat;
} mbe_parms;

typedef struct {
    unsigned int state[64];
    unsigned int index;
} AVLFG;

/*
 * Inline functions.
 */
static inline void mbe_moveMbeParms (mbe_parms * cur_mp, mbe_parms * prev_mp)
{
  int l;

  prev_mp->w0 = cur_mp->w0;
  prev_mp->L = cur_mp->L;
  prev_mp->K = cur_mp->K;       // necessary?
  prev_mp->Ml[0] = (float) 0;
  prev_mp->gamma = cur_mp->gamma;
  prev_mp->repeat = cur_mp->repeat;
  for (l = 0; l <= 56; l++) {
      prev_mp->Ml[l] = cur_mp->Ml[l];
      prev_mp->Vl[l] = cur_mp->Vl[l];
      prev_mp->log2Ml[l] = cur_mp->log2Ml[l];
      prev_mp->PHIl[l] = cur_mp->PHIl[l];
      prev_mp->PSIl[l] = cur_mp->PSIl[l];
  }
}

static inline void mbe_useLastMbeParms (mbe_parms * cur_mp, mbe_parms * prev_mp)
{
  int l;

  cur_mp->w0 = prev_mp->w0;
  cur_mp->L = prev_mp->L;
  cur_mp->K = prev_mp->K;       // necessary?
  cur_mp->Ml[0] = (float) 0;
  cur_mp->gamma = prev_mp->gamma;
  cur_mp->repeat = prev_mp->repeat;
  for (l = 0; l <= 56; l++) {
      cur_mp->Ml[l] = prev_mp->Ml[l];
      cur_mp->Vl[l] = prev_mp->Vl[l];
      cur_mp->log2Ml[l] = prev_mp->log2Ml[l];
      cur_mp->PHIl[l] = prev_mp->PHIl[l];
      cur_mp->PSIl[l] = prev_mp->PSIl[l];
  }
}

/*
 * Prototypes from ambe3600x2400.c
 */
void mbe_processAmbe2400Dataf (float *aout_buf, int *errs2, char *err_str, char ambe_d[49],
                               mbe_parms * cur_mp, mbe_parms * prev_mp, mbe_parms * prev_mp_enhanced, unsigned int uvquality);

/*
 * Prototypes from ambe3600x2450.c
 */
void mbe_processAmbe2450Dataf (float *aout_buf, int *errs2, char *err_str, char ambe_d[49],
                               mbe_parms * cur_mp, mbe_parms * prev_mp, mbe_parms * prev_mp_enhanced, unsigned int uvquality);

/*
 * Prototypes from imbe7200x4400.c
 */
void mbe_processImbe4400Dataf (float *aout_buf, int *errs2, char *err_str, char imbe_d[88],
                               mbe_parms * cur_mp, mbe_parms * prev_mp, mbe_parms * prev_mp_enhanced, int uvquality);

/*
 * Prototypes from mbelib.c
 */
extern const float AmbeLog2f[128];
float mbe_expf(float x);
float mbe_sqrtf(float x);
float mbe_cosf(float x);
void mbe_printVersion (char *str);
void mbe_initMbeParms (mbe_parms * cur_mp, mbe_parms * prev_mp, mbe_parms * prev_mp_enhanced);
void mbe_spectralAmpEnhance (mbe_parms * cur_mp);
void mbe_synthesizeSilencef (float *aout_buf);
int mbe_synthesizeSpeechf (float *aout_buf, mbe_parms * cur_mp, mbe_parms * prev_mp, int uvquality);

#endif
