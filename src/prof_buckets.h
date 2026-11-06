/*
 * Cubetas del perfilado por etapas (etapa 3). Compartido por decoder_core y por
 * el parche de libmad (third_party/patches/), para que ambos usen la misma
 * numeración.
 *
 * Las cubetas de libmad se rellenan desde marcas dentro de layer3.c: cada marca
 * suma a su cubeta el tiempo transcurrido desde la marca anterior. Huffman y
 * requantización van juntas porque libmad requantiza dentro de III_huffdecode.
 */
#ifndef MP368K_PROF_BUCKETS_H
#define MP368K_PROF_BUCKETS_H

enum {
    PROF_SIDE = 0,      /* cabecera ya leída: CRC, side info, copia de main_data */
    PROF_SCALEF,        /* scalefactors */
    PROF_HUFF,          /* Huffman + requantización */
    PROF_STEREO,        /* estéreo MS / intensidad */
    PROF_REORDER,       /* reorden de bloques cortos */
    PROF_ALIAS,         /* reducción de alias */
    PROF_IMDCT,         /* IMDCT + overlap + inversión de frecuencias */
    /* a partir de aquí las rellena decoder_core */
    PROF_OTHER,         /* resto de mad_frame_decode: sincronía, cabecera, bits */
    PROF_MONOMIX,       /* mezcla L+R del perfil MONO */
    PROF_SYNTH,         /* síntesis polifásica (dct32 + ventana) */
    PROF_PCM,           /* conversión a PCM de 16 bits */
    PROF_COUNT
};

#define PROF_LIBMAD_BUCKETS (PROF_IMDCT + 1)

#endif
