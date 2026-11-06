/*
 * mp368k_prof.h — marcas de perfilado para layer3.c (parte de MP368K, GPL).
 * Solo se incluye en la copia parcheada de libmad (tools/prepare-libmad-prof.sh).
 */
#ifndef MP368K_PROF_H
#define MP368K_PROF_H

#include "prof_buckets.h"

#ifdef MP368K_PROFILE

extern unsigned long long (*mp368k_prof_clock)(void);
extern unsigned long long mp368k_prof_last;
extern unsigned long mp368k_prof_acc[PROF_LIBMAD_BUCKETS];
extern unsigned long mp368k_prof_marks;

/* Suma a la cubeta b el tiempo desde la marca anterior. Sin reloj: no hace nada. */
# define MPPROF(b)  do { \
    if (mp368k_prof_clock) { \
      unsigned long long mp_now_ = mp368k_prof_clock(); \
      mp368k_prof_acc[b] += (unsigned long)(mp_now_ - mp368k_prof_last); \
      mp368k_prof_last = mp_now_; \
      ++mp368k_prof_marks; \
    } \
  } while (0)
# define MPPROF_RESET()  do { \
    if (mp368k_prof_clock) mp368k_prof_last = mp368k_prof_clock(); \
  } while (0)

#else
# define MPPROF(b)      ((void) 0)
# define MPPROF_RESET() ((void) 0)
#endif

#endif
