/*
 * asfparse — lector de ASF/WMA: cabecera (propiedades del archivo y del flujo de audio, etiquetas) y paquetes de datos.
 *
 * Portable (sin Toolbox). La E/S llega por los callbacks de un CodecIO. No usa malloc: el buffer de paquete lo
 * reserva quien lo llama. El formato ASF sigue la especificación pública de Microsoft; la lógica de los campos de longitud
 * variable de los paquetes sigue el enfoque de libasf (Juho Vähä-Herttua) tal como lo usa Rockbox.
 *
 * Un "objeto de medios" es lo que el decodificador de WMA llama superframe: block_align bytes. Un paquete ASF puede
 * traer varios, o un objeto puede venir partido entre paquetes; asf_next_object los entrega completos.
 */
#ifndef MP368K_ASFPARSE_H
#define MP368K_ASFPARSE_H

#include <stdint.h>

#include "codec.h"

#define ASF_CODEC_WMAV1 0x160
#define ASF_CODEC_WMAV2 0x161
#define ASF_CODEC_WMAPRO 0x162
#define ASF_EXTRA_MAX 46             /* bytes de datos extra del formato que se conservan */
#define ASF_MAX_PACKET 65536L

typedef struct {
    uint16_t codec_id;
    uint16_t channels;
    uint32_t rate;
    uint32_t bitrate;                /* bits por segundo (promedio indicado en la cabecera) */
    uint16_t blockalign;
    uint16_t bitspersample;
    uint16_t extra_len;
    uint8_t extra[ASF_EXTRA_MAX];
    int audiostream;                 /* número de flujo del audio */
    uint32_t packet_size;
    uint64_t num_packets;
    long data_offset;                /* byte del archivo donde empieza el primer paquete de datos */
    unsigned long duration_ms;       /* 0 si se desconoce */
    unsigned long preroll_ms;
    int has_audio;
    Id3Tags tags;
} AsfInfo;

/* Lee la cabecera. `buf` (>= 4096 bytes) es memoria de trabajo. CODEC_OK, o un CODEC_ERR_*. */
int asf_parse_header(const CodecIO *io, uint8_t *buf, long cap, AsfInfo *ai);

typedef struct {
    const CodecIO *io;
    const AsfInfo *ai;
    uint8_t *pkt;                    /* paquete actual (ai->packet_size bytes) */
    uint8_t *obj;                    /* objeto en reensamblado (obj_cap bytes) */
    long obj_cap;
    uint64_t next_packet;            /* índice del próximo paquete a leer */
    /* estado de la recorrida de cargas del paquete actual */
    long cursor;                     /* próximo byte por analizar dentro de pkt */
    long end;                        /* fin de las cargas en pkt */
    unsigned payloads_left;
    unsigned multiple;
    unsigned payload_length_type;
    unsigned prop;
    uint32_t padding;
    /* reensamblado de un objeto partido entre cargas */
    int asm_active;
    uint32_t asm_number, asm_size, asm_have;
    uint32_t asm_pts;                /* marca de tiempo del objeto en reensamblado */
    uint32_t pts_ms;                 /* hora de envío del paquete actual */
    int errors;
} AsfReader;

void asf_reader_init(AsfReader *r, const CodecIO *io, const AsfInfo *ai, uint8_t *pkt, uint8_t *obj, long obj_cap);

/*
 * Siguiente objeto de medios completo del flujo de audio. Devuelve 1 con *data y *len (válido hasta la próxima llamada) y
 * *pts_ms = su marca de tiempo de presentación en ms (incluye el preroll); 0 al terminar el archivo y -1 si hay un error del
 * que no se pudo recuperar.
 */
int asf_next_object(AsfReader *r, const uint8_t **data, long *len, uint32_t *pts_ms);

/* Reposiciona unos paquetes antes de `ms` (sin el preroll): el objeto siguiente trae su hora de presentación. 0 si lo logró. */
int asf_seek_ms(AsfReader *r, unsigned long ms);

#endif
