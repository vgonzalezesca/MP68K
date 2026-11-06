/*
 * mp368k_prof.c — acumuladores de las marcas de perfilado (parte de MP368K, GPL).
 */
#include "mp368k_prof.h"

#ifdef MP368K_PROFILE
unsigned long long (*mp368k_prof_clock)(void);
unsigned long long mp368k_prof_last;
unsigned long mp368k_prof_acc[PROF_LIBMAD_BUCKETS];
unsigned long mp368k_prof_marks;
#endif
