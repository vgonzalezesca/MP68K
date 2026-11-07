/*
 * codec_vorbis — Ogg Vorbis con Tremor (rama lowmem, BSD de 3 cláusulas), en punto fijo y sobre un heap sin malloc.
 *
 * El reconocimiento, las etiquetas y la duración (probe) se leen con un lector de páginas Ogg propio, sin Tremor y sin
 * memoria de códec: así agregar una pista a la lista mientras suena otra no toca la memoria del decodificador. Para
 * reproducir se abre Tremor sobre un heap (src/heap.c) que se reparte dentro del arena del códec; las asignaciones de
 * Tremor llegan por mp368k_tremor_malloc y compañía (third_party/patches/mp368k_tremor.h).
 *
 * Límites declarados: hasta 2 canales y de 8 a 48 kHz (el audio de hoy no remuestrea); solo el primer flujo si el
 * archivo trae varios encadenados. Los comentarios y la duración salen del primer flujo.
 */
#include "codec.h"

#include <setjmp.h>
#include <string.h>

#include "heap.h"
#include "ivorbisfile.h"
#include "vorbiscomment.h"

#define MIN_RATE 8000
#define MAX_RATE 48000
#define MAX_PAGES 512               /* páginas que se recorren para armar el paquete de comentarios */
#define MAX_COMMENT 4096            /* tope de entradas de comentario */
#define MAX_ENTRY 512               /* entradas más largas se saltan (carátulas en base64, etc.) */
#define TAIL_SCAN (64L * 1024)      /* ventana final donde se busca la última página con posición de gránulo */
#define MAX_HOLES 16                /* huecos seguidos que se toleran antes de rendirse */

static OggVorbis_File sVf;
static int sOpen;
static Heap sHeap;
static CodecIO sIo;
static long sPos;                   /* posición de lectura actual (tell) */
static int16_t sPcm[CODEC_MAX_FRAMES * 2];
static unsigned sRate, sCh;

/*
 * Tremor no revisa todas sus asignaciones. Si el heap se agota dentro de una llamada a Tremor se sale con longjmp al
 * punto de entrada del códec, que informa CODEC_ERR_MEMORY. El estado de Tremor queda a medias, así que después de
 * eso no se vuelve a llamar a Tremor (sOpen = 0); la memoria es del arena, se recupera en el próximo arena_reset.
 */
static jmp_buf sOom;
static int sGuard;

static void oom(void)
{
    if (sGuard) {
        sGuard = 0;
        longjmp(sOom, 1);
    }
}

/* ------------------------------------------------------------------------ */
/* Memoria de Tremor                                                         */
/* ------------------------------------------------------------------------ */

void *mp368k_tremor_malloc(size_t n)
{
    void *p = heap_alloc(&sHeap, n);

    if (p == NULL)
        oom();
    return p;
}

void *mp368k_tremor_calloc(size_t count, size_t size)
{
    void *p = heap_calloc(&sHeap, count, size);

    if (p == NULL)
        oom();
    return p;
}

void *mp368k_tremor_realloc(void *p, size_t n)
{
    void *q = heap_realloc(&sHeap, p, n);

    if (q == NULL)
        oom();
    return q;
}

void mp368k_tremor_free(void *p)
{
    heap_free(&sHeap, p);
}

/* ------------------------------------------------------------------------ */
/* E/S de Tremor sobre CodecIO                                               */
/* ------------------------------------------------------------------------ */

static size_t cb_read(void *ptr, size_t size, size_t nmemb, void *ds)
{
    long got = 0, want = (long)(size * nmemb);

    (void)ds;
    if (size == 0 || nmemb == 0)
        return 0;
    if (sIo.read(sIo.ctx, ptr, want, &got) != 0 || got < 0)
        return 0;
    sPos += got;
    return (size_t)got / size;
}

static int cb_seek(void *ds, ogg_int64_t offset, int whence)
{
    long target;

    (void)ds;
    if (whence == SEEK_SET)
        target = (long)offset;
    else if (whence == SEEK_CUR)
        target = sPos + (long)offset;
    else
        target = sIo.size + (long)offset;
    if (target < 0 || sIo.seek(sIo.ctx, target) != 0)
        return -1;
    sPos = target;
    return 0;
}

static int cb_close(void *ds)
{
    (void)ds;
    return 0;
}

static long cb_tell(void *ds)
{
    (void)ds;
    return sPos;
}

/* ------------------------------------------------------------------------ */
/* Lector de páginas Ogg para el reconocimiento                              */
/* ------------------------------------------------------------------------ */

static unsigned long le32(const uint8_t *p)
{
    return (unsigned long)p[0] | ((unsigned long)p[1] << 8) | ((unsigned long)p[2] << 16) | ((unsigned long)p[3] << 24);
}

/* Lee exactamente `want` bytes desde `offset`; 0 si lo logró. */
static int rd(const CodecIO *io, long offset, uint8_t *dst, long want)
{
    long total = 0, n;

    if (io->seek(io->ctx, offset) != 0)
        return -1;
    while (total < want) {
        n = 0;
        if (io->read(io->ctx, dst + total, want - total, &n) != 0 || n <= 0)
            return -1;
        total += n;
    }
    return 0;
}

typedef struct {
    long data_pos;                  /* byte del archivo donde empieza la carga de la página */
    long data_len;
    unsigned flags;                 /* bit 0: continúa un paquete; bit 1: primera del flujo; bit 2: última */
    unsigned nseg;
    uint8_t seg[255];
    unsigned long serial;
    int has_granule;                /* 0 si la posición de gránulo vale -1 */
    unsigned long long granule;
} OggPage;

/* Lee la página que empieza en `pos`. 0 si es válida. */
static int page_at(const CodecIO *io, long pos, OggPage *pg)
{
    uint8_t h[27];
    unsigned i;
    long len = 0;

    if (rd(io, pos, h, 27) != 0 || memcmp(h, "OggS", 4) != 0 || h[4] != 0)
        return -1;
    pg->flags = h[5];
    pg->nseg = h[26];
    pg->serial = le32(h + 14);
    pg->has_granule = !(h[6] == 0xFF && h[7] == 0xFF && h[8] == 0xFF && h[9] == 0xFF &&
                        h[10] == 0xFF && h[11] == 0xFF && h[12] == 0xFF && h[13] == 0xFF);
    pg->granule = (unsigned long long)le32(h + 6) | ((unsigned long long)le32(h + 10) << 32);
    if (pg->nseg && rd(io, pos + 27, pg->seg, (long)pg->nseg) != 0)
        return -1;
    for (i = 0; i < pg->nseg; i++)
        len += pg->seg[i];
    pg->data_pos = pos + 27 + (long)pg->nseg;
    pg->data_len = len;
    return 0;
}

/* Un paquete repartido en trozos contiguos por página: permite leer en cualquier desplazamiento del paquete. */
typedef struct {
    long pos[MAX_PAGES];
    long len[MAX_PAGES];
    unsigned n;
    long total;
} Packet;

/* Arma el paquete que empieza en la carga de `pg` (su primer segmento), leyendo páginas siguientes si continúa. */
static int packet_from(const CodecIO *io, long page_pos, OggPage *pg, Packet *pk)
{
    pk->n = 0;
    pk->total = 0;
    for (;;) {
        unsigned i;
        long take = 0;
        int done = 0;

        for (i = 0; i < pg->nseg; i++) {
            take += pg->seg[i];
            if (pg->seg[i] < 255) {
                done = 1;
                break;
            }
        }
        if (pk->n >= MAX_PAGES)
            return -1;
        pk->pos[pk->n] = pg->data_pos;
        pk->len[pk->n] = take;
        pk->n++;
        pk->total += take;
        if (done)
            return 0;
        page_pos = pg->data_pos + pg->data_len;
        if (page_at(io, page_pos, pg) != 0 || !(pg->flags & 1))
            return -1;
    }
}

static int packet_read(const CodecIO *io, const Packet *pk, long offset, uint8_t *dst, long want)
{
    unsigned i;
    long base = 0;

    if (offset < 0 || offset + want > pk->total)
        return -1;
    for (i = 0; i < pk->n && want > 0; i++) {
        if (offset < base + pk->len[i]) {
            long in = offset - base, n = pk->len[i] - in;

            if (n > want)
                n = want;
            if (rd(io, pk->pos[i] + in, dst, n) != 0)
                return -1;
            dst += n;
            offset += n;
            want -= n;
        }
        base += pk->len[i];
    }
    return want == 0 ? 0 : -1;
}

/* Etiquetas del paquete de comentarios (tipo 3). Tolera paquetes dañados. */
static void read_comments(const CodecIO *io, const Packet *pk, uint8_t *buf, long cap, Id3Tags *tg)
{
    uint8_t w[4];
    long cur = 7;
    unsigned long vendor, count, i;

    if (packet_read(io, pk, 0, buf, 7) != 0 || buf[0] != 3 || memcmp(buf + 1, "vorbis", 6) != 0)
        return;
    if (packet_read(io, pk, cur, w, 4) != 0)
        return;
    vendor = le32(w);
    cur += 4 + (long)vendor;
    if (cur + 4 > pk->total || packet_read(io, pk, cur, w, 4) != 0)
        return;
    count = le32(w);
    cur += 4;
    if (count > MAX_COMMENT)
        count = MAX_COMMENT;
    for (i = 0; i < count && cur + 4 <= pk->total; i++) {
        unsigned long n;

        if (packet_read(io, pk, cur, w, 4) != 0)
            return;
        n = le32(w);
        cur += 4;
        if (cur + (long)n > pk->total)
            return;
        if (n <= MAX_ENTRY && (long)n <= cap && packet_read(io, pk, cur, buf, (long)n) == 0)
            vorbiscomment_entry(buf, (size_t)n, tg);
        cur += (long)n;
    }
}

/* Posición de gránulo de la última página con valor, buscando en la ventana final. 0 si no la halla. */
static unsigned long long last_granule(const CodecIO *io, uint8_t *buf, long cap, unsigned long serial)
{
    long start = io->size > TAIL_SCAN ? io->size - TAIL_SCAN : 0;
    long step = cap - 32, off, n, k;
    unsigned long long best = 0;

    for (off = start; off < io->size; off += step) {
        n = io->size - off < cap ? io->size - off : cap;
        if (rd(io, off, buf, n) != 0)
            return best;
        for (k = 0; k + 27 <= n && k < step; k++) {
            if (buf[k] == 'O' && buf[k + 1] == 'g' && buf[k + 2] == 'g' && buf[k + 3] == 'S' && buf[k + 4] == 0 &&
                le32(buf + k + 14) == serial &&
                !(buf[k + 6] == 0xFF && buf[k + 7] == 0xFF && buf[k + 8] == 0xFF && buf[k + 9] == 0xFF &&
                  buf[k + 10] == 0xFF && buf[k + 11] == 0xFF && buf[k + 12] == 0xFF && buf[k + 13] == 0xFF))
                best = (unsigned long long)le32(buf + k + 6) | ((unsigned long long)le32(buf + k + 10) << 32);
        }
    }
    return best;
}

static int vorbis_sniff(const uint8_t *head, size_t n)
{
    return n >= 4 && memcmp(head, "OggS", 4) == 0;
}

static int vorbis_probe(const CodecIO *io, uint8_t *buf, long cap, CodecInfo *ci)
{
    static OggPage pg;
    static Packet pk;
    uint8_t id[30];
    long pos = io->audio_start, next;
    unsigned long serial, nominal;
    unsigned long long total;

    if (cap < MAX_ENTRY)
        return CODEC_ERR_PARAMS;
    if (page_at(io, pos, &pg) != 0)
        return CODEC_ERR_UNSUPPORTED;
    if (!(pg.flags & 2) || packet_from(io, pos, &pg, &pk) != 0 || pk.total < 30)
        return CODEC_ERR_UNSUPPORTED;
    if (packet_read(io, &pk, 0, id, 30) != 0 || id[0] != 1 || memcmp(id + 1, "vorbis", 6) != 0 || le32(id + 7) != 0)
        return CODEC_ERR_UNSUPPORTED;            /* no es Vorbis (Opus, FLAC en Ogg, Speex...) */
    serial = pg.serial;
    ci->channels = id[11];
    ci->rate = (unsigned)le32(id + 12);
    nominal = le32(id + 20);
    if (ci->channels == 0 || ci->rate == 0)
        return CODEC_ERR_UNSUPPORTED;

    next = pg.data_pos + pg.data_len;
    if (page_at(io, next, &pg) == 0 && pg.serial == serial && packet_from(io, next, &pg, &pk) == 0)
        read_comments(io, &pk, buf, cap, &ci->tags);

    if (ci->channels > 2 || ci->rate < MIN_RATE || ci->rate > MAX_RATE)
        return CODEC_ERR_PARAMS;

    total = last_granule(io, buf, cap, serial);
    strcpy(ci->format, "Vorbis");
    ci->vbr = 1;
    ci->bits_src = 0;
    ci->duration_ms = total ? (unsigned long)((total / ci->rate) * 1000ULL + ((total % ci->rate) * 1000ULL) / ci->rate) : 0;
    if (ci->duration_ms)
        ci->bitrate = (unsigned)((unsigned long long)(io->size - io->audio_start) * 8000ULL / ci->duration_ms);
    else
        ci->bitrate = (unsigned)(nominal & 0x7FFFFFFFUL);
    ci->has_profiles = 0;
    ci->eq_capable = 0;
    return CODEC_OK;
}

/* ------------------------------------------------------------------------ */
/* Reproducción                                                              */
/* ------------------------------------------------------------------------ */

static int vorbis_open(const CodecIO *io, DecProfile prof, uint8_t *buf, size_t bufsize, const CodecInfo *ci)
{
    static const ov_callbacks cbs = { cb_read, cb_seek, cb_close, cb_tell };
    Arena *a = codec_arena();
    size_t room;
    void *mem;
    vorbis_info *vi;
    int rc;

    (void)prof;
    (void)buf;
    (void)bufsize;
    (void)ci;
    arena_reset(a);
    room = arena_remaining(a);
    mem = room ? arena_alloc(a, room) : NULL;
    if (mem == NULL)
        return CODEC_ERR_MEMORY;
    heap_init(&sHeap, mem, room);
    sIo = *io;
    sPos = 0;
    if (sIo.seek(sIo.ctx, 0) != 0)
        return CODEC_ERR_IO;
    memset(&sVf, 0, sizeof sVf);
    sGuard = 1;
    if (setjmp(sOom) == 0)
        rc = ov_open_callbacks(&sIo, &sVf, NULL, 0, cbs);   /* datasource no nulo: Tremor lo usa para saber si se puede buscar */
    else
        rc = -1000;                                         /* sin memoria */
    sGuard = 0;
    if (rc != 0)
        return (rc == -1000 || sHeap.failed) ? CODEC_ERR_MEMORY : CODEC_ERR_UNSUPPORTED;
    sOpen = 1;
    vi = ov_info(&sVf, -1);
    sRate = vi ? (unsigned)vi->rate : 0;
    sCh = vi ? (unsigned)vi->channels : 0;
    if (sCh < 1 || sCh > 2 || sRate < MIN_RATE || sRate > MAX_RATE) {
        ov_clear(&sVf);
        sOpen = 0;
        return CODEC_ERR_PARAMS;
    }
    return CODEC_OK;
}

static int vorbis_next(DecFrame *out)
{
    int holes = 0, bs = 0;
    long bytes;

    if (!sOpen)
        return -1;
    for (;;) {
        sGuard = 1;
        if (setjmp(sOom) == 0) {
            bytes = ov_read(&sVf, sPcm, (int)(CODEC_MAX_FRAMES * sCh * 2), &bs);
        } else {
            sOpen = 0;                      /* Tremor quedó a medias: no se vuelve a usar */
            return -1;
        }
        sGuard = 0;
        if (bytes > 0)
            break;
        if (bytes == 0)
            return 0;                       /* fin del archivo */
        if (bytes == OV_HOLE && ++holes <= MAX_HOLES)
            continue;                       /* datos perdidos o dañados: se sigue con lo que venga */
        return -1;
    }
    if (bs != 0)
        return 0;                           /* un segundo flujo encadenado: no se reproduce */
    out->pcm = sPcm;
    out->frames = (unsigned)(bytes / (2 * (long)sCh));
    out->count = out->frames * sCh;
    out->channels = sCh;
    out->rate = sRate;
    out->bitrate = (unsigned)ov_bitrate_instant(&sVf);
    out->in_rate = sRate;
    out->in_channels = sCh;
    out->layer = 0;
    out->file_pos = sPos;
    return 1;
}

static int vorbis_seek(unsigned long ms)
{
    int rc;

    if (!sOpen)
        return CODEC_ERR_IO;
    sGuard = 1;
    if (setjmp(sOom) == 0) {
        rc = ov_time_seek(&sVf, (ogg_int64_t)ms);
    } else {
        sOpen = 0;
        return CODEC_ERR_MEMORY;
    }
    sGuard = 0;
    return rc == 0 ? CODEC_OK : CODEC_ERR_IO;
}

static void vorbis_close(void)
{
    if (sOpen) {
        ov_clear(&sVf);
        sOpen = 0;
    }
}

static size_t vorbis_mem_peak(void)
{
    return sHeap.peak;
}

const Codec codec_vorbis = { "Vorbis", vorbis_sniff, vorbis_probe, vorbis_open, vorbis_next, vorbis_seek, vorbis_close,
                             vorbis_mem_peak };
