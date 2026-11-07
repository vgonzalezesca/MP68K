/*
 * asfparse — ver asfparse.h.
 */
#include "asfparse.h"

#include <string.h>

static const uint8_t kGuidHeader[16] = { 0x30, 0x26, 0xB2, 0x75, 0x8E, 0x66, 0xCF, 0x11, 0xA6, 0xD9, 0x00, 0xAA, 0x00, 0x62, 0xCE, 0x6C };
static const uint8_t kGuidData[16] = { 0x36, 0x26, 0xB2, 0x75, 0x8E, 0x66, 0xCF, 0x11, 0xA6, 0xD9, 0x00, 0xAA, 0x00, 0x62, 0xCE, 0x6C };
static const uint8_t kGuidFileProps[16] = { 0xA1, 0xDC, 0xAB, 0x8C, 0x47, 0xA9, 0xCF, 0x11, 0x8E, 0xE4, 0x00, 0xC0, 0x0C, 0x20, 0x53, 0x65 };
static const uint8_t kGuidStreamProps[16] = { 0x91, 0x07, 0xDC, 0xB7, 0xB7, 0xA9, 0xCF, 0x11, 0x8E, 0xE6, 0x00, 0xC0, 0x0C, 0x20, 0x53, 0x65 };
static const uint8_t kGuidContentDesc[16] = { 0x33, 0x26, 0xB2, 0x75, 0x8E, 0x66, 0xCF, 0x11, 0xA6, 0xD9, 0x00, 0xAA, 0x00, 0x62, 0xCE, 0x6C };
static const uint8_t kGuidExtContent[16] = { 0x40, 0xA4, 0xD0, 0xD2, 0x07, 0xE3, 0xD2, 0x11, 0x97, 0xF0, 0x00, 0xA0, 0xC9, 0x5E, 0xA8, 0x50 };
static const uint8_t kGuidAudio[16] = { 0x40, 0x9E, 0x69, 0xF8, 0x4D, 0x5B, 0xCF, 0x11, 0xA8, 0xFD, 0x00, 0x80, 0x5F, 0x5C, 0x44, 0x2B };

#define MAX_OBJECTS 64              /* objetos de cabecera que se recorren */
#define MAX_ATTRS 256               /* atributos de la descripción extendida que se recorren */
#define MAX_TAG_BYTES 256           /* bytes UTF-16 que se leen de una etiqueta */

static unsigned le16(const uint8_t *p)
{
    return (unsigned)p[0] | ((unsigned)p[1] << 8);
}

static unsigned long le32(const uint8_t *p)
{
    return (unsigned long)p[0] | ((unsigned long)p[1] << 8) | ((unsigned long)p[2] << 16) | ((unsigned long)p[3] << 24);
}

static unsigned long long le64(const uint8_t *p)
{
    return (unsigned long long)le32(p) | ((unsigned long long)le32(p + 4) << 32);
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

/* UTF-16LE (hasta el primer NUL o `n` bytes) -> MacRoman. Los pares sustitutos y lo que MacRoman no tiene quedan '?'. */
static void utf16_to_macroman(const uint8_t *s, long n, char *dst, size_t cap)
{
    size_t o = 0;
    long i;

    if (cap == 0)
        return;
    for (i = 0; i + 1 < n && o + 1 < cap; i += 2) {
        unsigned cp = le16(s + i);

        if (cp == 0)
            break;
        dst[o++] = (cp >= 0xD800 && cp < 0xE000) ? '?' : (char)id3_cp_to_macroman(cp);
    }
    dst[o] = '\0';
}

/* Etiqueta de `len` bytes desde `pos`; completa solo si el destino está vacío. */
static void read_tag(const CodecIO *io, long pos, long len, uint8_t *buf, char *dst, size_t cap)
{
    if (len <= 0 || dst[0] != '\0')
        return;
    if (len > MAX_TAG_BYTES)
        len = MAX_TAG_BYTES;
    if (rd(io, pos, buf, len) == 0)
        utf16_to_macroman(buf, len, dst, cap);
}

static void content_description(const CodecIO *io, long pos, long size, uint8_t *buf, Id3Tags *tg)
{
    unsigned len[5], i;
    long cur;

    if (size < 10 || rd(io, pos, buf, 10) != 0)
        return;
    for (i = 0; i < 5; i++)
        len[i] = le16(buf + 2 * i);
    cur = pos + 10;
    read_tag(io, cur, (long)len[0], buf, tg->title, sizeof tg->title);
    cur += (long)len[0];
    read_tag(io, cur, (long)len[1], buf, tg->artist, sizeof tg->artist);
}

static int utf16_equals(const uint8_t *s, long n, const char *ascii)
{
    long i = 0;

    for (; ascii[i]; i++)
        if (2 * i + 1 >= n || le16(s + 2 * i) != (unsigned char)ascii[i])
            return 0;
    return n == 2 * i || (n == 2 * i + 2 && le16(s + 2 * i) == 0);
}

/* WM/AlbumTitle y, si falta el artista, WM/AlbumArtist, de la descripción extendida. */
static void ext_content(const CodecIO *io, long pos, long size, uint8_t *buf, long cap, Id3Tags *tg)
{
    long cur = pos, end = pos + size;
    unsigned count, i;

    if (size < 2 || rd(io, cur, buf, 2) != 0)
        return;
    count = le16(buf);
    cur += 2;
    if (count > MAX_ATTRS)
        count = MAX_ATTRS;
    for (i = 0; i < count && cur + 2 <= end; i++) {
        unsigned nlen, type, vlen;
        long nstart, vstart;
        int album, artist;

        if (rd(io, cur, buf, 2) != 0)
            return;
        nlen = le16(buf);
        nstart = cur + 2;
        if (nstart + (long)nlen + 4 > end)
            return;
        album = artist = 0;
        if (nlen <= 64 && (long)nlen <= cap && rd(io, nstart, buf, (long)nlen) == 0) {
            album = utf16_equals(buf, (long)nlen, "WM/AlbumTitle");
            artist = utf16_equals(buf, (long)nlen, "WM/AlbumArtist");
        }
        if (rd(io, nstart + (long)nlen, buf, 4) != 0)
            return;
        type = le16(buf);
        vlen = le16(buf + 2);
        vstart = nstart + (long)nlen + 4;
        if (vstart + (long)vlen > end)
            return;
        if (type == 0 && album)
            read_tag(io, vstart, (long)vlen, buf, tg->album, sizeof tg->album);
        else if (type == 0 && artist)
            read_tag(io, vstart, (long)vlen, buf, tg->artist, sizeof tg->artist);
        cur = vstart + (long)vlen;
    }
}

static void stream_properties(const CodecIO *io, long pos, long size, uint8_t *buf, long cap, AsfInfo *ai)
{
    unsigned long tlen;
    unsigned stream;

    if (size < 54 || cap < 128 || rd(io, pos, buf, 54) != 0)
        return;
    if (memcmp(buf, kGuidAudio, 16) != 0 || ai->has_audio)
        return;
    tlen = le32(buf + 40);
    stream = le16(buf + 48) & 0x7F;
    if (tlen < 18 || (long)tlen > size - 54 || tlen > (unsigned long)cap || rd(io, pos + 54, buf, (long)tlen) != 0)
        return;
    ai->codec_id = (uint16_t)le16(buf);
    ai->channels = (uint16_t)le16(buf + 2);
    ai->rate = le32(buf + 4);
    ai->bitrate = le32(buf + 8) * 8;
    ai->blockalign = (uint16_t)le16(buf + 12);
    ai->bitspersample = (uint16_t)le16(buf + 14);
    ai->extra_len = (uint16_t)le16(buf + 16);
    if ((unsigned long)ai->extra_len + 18 > tlen)
        ai->extra_len = (uint16_t)(tlen - 18);
    if (ai->extra_len > ASF_EXTRA_MAX)
        ai->extra_len = ASF_EXTRA_MAX;
    memcpy(ai->extra, buf + 18, ai->extra_len);
    ai->audiostream = (int)stream;
    ai->has_audio = 1;
}

int asf_parse_header(const CodecIO *io, uint8_t *buf, long cap, AsfInfo *ai)
{
    long base = io->audio_start, pos, end;
    unsigned long long hdr_size, nobj, i;
    int have_props = 0;

    if (cap < 4096)
        return CODEC_ERR_PARAMS;
    memset(ai, 0, sizeof *ai);
    if (rd(io, base, buf, 30) != 0)
        return CODEC_ERR_IO;
    if (memcmp(buf, kGuidHeader, 16) != 0)
        return CODEC_ERR_UNSUPPORTED;
    hdr_size = le64(buf + 16);
    nobj = le32(buf + 24);
    if (hdr_size < 30 || hdr_size > (unsigned long long)(io->size - base))
        return CODEC_ERR_UNSUPPORTED;
    end = base + (long)hdr_size;
    pos = base + 30;
    for (i = 0; i < nobj && i < MAX_OBJECTS && pos + 24 <= end; i++) {
        unsigned long long size;

        if (rd(io, pos, buf, 24) != 0)
            return CODEC_ERR_IO;
        size = le64(buf + 16);
        if (size < 24 || size > (unsigned long long)(end - pos))
            break;
        if (memcmp(buf, kGuidFileProps, 16) == 0 && size >= 24 + 80) {
            uint8_t fp[80];

            if (rd(io, pos + 24, fp, 80) != 0)
                return CODEC_ERR_IO;
            {
                unsigned long long play = le64(fp + 40) / 10000ULL, pre = le64(fp + 56);

                ai->preroll_ms = (unsigned long)pre;
                ai->duration_ms = play > pre ? (unsigned long)(play - pre) : 0;
            }
            ai->packet_size = le32(fp + 72);
            ai->num_packets = le64(fp + 32);
            have_props = 1;
        } else if (memcmp(buf, kGuidStreamProps, 16) == 0) {
            stream_properties(io, pos + 24, (long)size - 24, buf, cap, ai);
        } else if (memcmp(buf, kGuidContentDesc, 16) == 0) {
            content_description(io, pos + 24, (long)size - 24, buf, &ai->tags);
        } else if (memcmp(buf, kGuidExtContent, 16) == 0) {
            ext_content(io, pos + 24, (long)size - 24, buf, cap, &ai->tags);
        }
        pos += (long)size;
    }
    if (!ai->has_audio || !have_props)
        return CODEC_ERR_UNSUPPORTED;
    if (ai->packet_size == 0 || ai->packet_size > ASF_MAX_PACKET)
        return CODEC_ERR_UNSUPPORTED;

    /* objeto de datos: GUID(16) tamaño(8) id de archivo(16) paquetes(8) reservado(2) */
    if (rd(io, end, buf, 50) != 0 || memcmp(buf, kGuidData, 16) != 0)
        return CODEC_ERR_UNSUPPORTED;
    ai->data_offset = end + 50;
    {
        unsigned long long total = le64(buf + 40);

        if (total != 0)
            ai->num_packets = total;
    }
    if (ai->num_packets == 0) {                 /* sin conteo: se deduce del tamaño del archivo */
        ai->num_packets = (unsigned long long)((io->size - ai->data_offset) / (long)ai->packet_size);
    } else if ((long long)ai->num_packets * (long long)ai->packet_size > (long long)(io->size - ai->data_offset)) {
        ai->num_packets = (unsigned long long)((io->size - ai->data_offset) / (long)ai->packet_size);   /* archivo truncado */
    }
    return CODEC_OK;
}

/* ------------------------------------------------------------------------ */
/* Paquetes de datos                                                         */
/* ------------------------------------------------------------------------ */

/* Bytes de un campo de longitud variable cuyo tipo (2 bits) es `t`: 0, 1, 2 o 4. */
static unsigned len2b(unsigned t)
{
    return t == 3 ? 4 : t;
}

static unsigned long val2b(unsigned t, const uint8_t *p)
{
    return t == 3 ? le32(p) : t == 2 ? le16(p) : t == 1 ? p[0] : 0;
}

void asf_reader_init(AsfReader *r, const CodecIO *io, const AsfInfo *ai, uint8_t *pkt, uint8_t *obj, long obj_cap)
{
    memset(r, 0, sizeof *r);
    r->io = io;
    r->ai = ai;
    r->pkt = pkt;
    r->obj = obj;
    r->obj_cap = obj_cap;
}

/* Interpreta la cabecera del paquete en `p` (de `size` bytes). Devuelve la posición de la primera carga, o -1. */
static long packet_header(const uint8_t *p, uint32_t avail, uint32_t size, unsigned *multiple, unsigned *plt,
                          unsigned *prop, uint32_t *padding, uint32_t *send_ms, uint32_t *length_out)
{
    long pos = 0;
    unsigned flags;
    unsigned long length;
    uint32_t pad;

    /* `avail` son los bytes de p que se pueden leer (al buscar posiciones solo se lee el comienzo); `size`, el tamaño fijo del paquete */
    if (avail < 16)
        return -1;
    if (p[0] & 0x80) {                              /* datos de corrección de errores */
        unsigned ec_len = p[0] & 0x0F;

        if (((p[0] >> 5) & 3) != 0 || (p[0] & 0x10) || ec_len != 2)
            return -1;
        pos = 1 + (long)ec_len;
    }
    flags = p[pos];
    *prop = p[pos + 1];
    pos += 2;
    {
        unsigned lt = (flags >> 5) & 3, st = (flags >> 1) & 3, pt = (flags >> 3) & 3;
        long need = (long)len2b(lt) + (long)len2b(st) + (long)len2b(pt) + 6;

        if (pos + need > (long)avail)
            return -1;
        length = lt ? val2b(lt, p + pos) : size;
        pos += (long)len2b(lt);
        pos += (long)len2b(st);                     /* secuencia: no se usa */
        pad = (uint32_t)val2b(pt, p + pos);
        pos += (long)len2b(pt);
        *send_ms = (uint32_t)le32(p + pos);
        pos += 6;                                   /* hora de envío (4) y duración (2) */
    }
    if (length < size) {                            /* paquete más corto que el tamaño fijo: el resto es relleno */
        pad += size - (uint32_t)length;
        length = size;
    }
    if (length > size)
        return -1;
    *padding = pad;
    *length_out = (uint32_t)length;
    *multiple = flags & 1;
    *plt = 2;
    if (*multiple) {
        if (pos >= (long)avail)
            return -1;
        *plt = (p[pos] >> 6) & 3;
        *multiple = (unsigned)(p[pos] & 0x3F) | 0x100;      /* cuenta de cargas en los 8 bits bajos, bandera en el bit 8 */
        pos++;
    }
    return pos;
}

static int load_next_packet(AsfReader *r)
{
    const AsfInfo *ai = r->ai;
    uint32_t length;
    long pos;
    unsigned mult;

    if (r->next_packet >= ai->num_packets)
        return 0;
    if (rd(r->io, ai->data_offset + (long)((unsigned long long)ai->packet_size * r->next_packet), r->pkt, (long)ai->packet_size) != 0)
        return 0;                                   /* fin del archivo, o truncado en mitad de un paquete */
    r->next_packet++;
    pos = packet_header(r->pkt, ai->packet_size, ai->packet_size, &mult, &r->payload_length_type, &r->prop, &r->padding,
                        &r->pts_ms, &length);
    if (pos < 0)
        return -1;
    r->multiple = mult >> 8;
    r->payloads_left = r->multiple ? (mult & 0xFF) : 1;
    r->cursor = pos;
    r->end = (long)length - (long)r->padding;
    if (r->end < pos || r->end > (long)ai->packet_size)
        return -1;
    return 1;
}

int asf_next_object(AsfReader *r, const uint8_t **data, long *len, uint32_t *pts_ms)
{
    const AsfInfo *ai = r->ai;

    for (;;) {
        const uint8_t *p = r->pkt;
        unsigned stream, nlen, olen, rlen_t, number_len, offset_len, plen_bytes;
        unsigned long number, offset, rlen, plen;
        long cur;

        if (r->payloads_left == 0) {
            int rc = load_next_packet(r);

            if (rc == 0)
                return 0;
            if (rc < 0) {
                if (++r->errors > 5)
                    return -1;
                r->payloads_left = 0;
            }
            continue;
        }
        cur = r->cursor;
        if (cur >= r->end)
            goto bad_packet;
        stream = p[cur++] & 0x7F;
        number_len = len2b((r->prop >> 4) & 3);
        offset_len = len2b((r->prop >> 2) & 3);
        rlen_t = r->prop & 3;
        nlen = number_len;
        olen = offset_len;
        if (cur + (long)nlen + (long)olen + (long)len2b(rlen_t) > r->end)
            goto bad_packet;
        number = val2b((r->prop >> 4) & 3, p + cur);
        cur += (long)nlen;
        offset = val2b((r->prop >> 2) & 3, p + cur);
        cur += (long)olen;
        rlen = val2b(rlen_t, p + cur);
        cur += (long)len2b(rlen_t);
        if (rlen == 1)                              /* cargas comprimidas: no se usan en audio */
            goto bad_packet;
        if (cur + (long)rlen > r->end)
            goto bad_packet;
        {
            unsigned long osize = rlen >= 8 ? le32(p + cur) : 0;
            uint32_t pts = rlen >= 8 ? (uint32_t)le32(p + cur + 4) : r->pts_ms;

            cur += (long)rlen;
            if (r->multiple) {
                plen_bytes = len2b(r->payload_length_type);
                if (plen_bytes == 0 || cur + (long)plen_bytes > r->end)
                    goto bad_packet;
                plen = val2b(r->payload_length_type, p + cur);
                cur += (long)plen_bytes;
            } else {
                plen = (unsigned long)(r->end - cur);
            }
            if (cur + (long)plen > r->end)
                goto bad_packet;
            r->cursor = cur + (long)plen;
            r->payloads_left--;

            if ((int)stream != ai->audiostream)
                continue;
            if (offset == 0 && (osize == 0 || plen == osize)) {     /* objeto completo en esta carga */
                r->asm_active = 0;
                r->errors = 0;
                *data = p + cur;
                *len = (long)plen;
                *pts_ms = pts;
                return 1;
            }
            if (offset == 0 && plen < osize) {                      /* empieza un objeto partido */
                if (osize > (unsigned long)r->obj_cap)
                    continue;
                memcpy(r->obj, p + cur, plen);
                r->asm_active = 1;
                r->asm_number = (uint32_t)number;
                r->asm_size = (uint32_t)osize;
                r->asm_have = (uint32_t)plen;
                r->asm_pts = pts;
                continue;
            }
            if (r->asm_active && r->asm_number == (uint32_t)number && offset == r->asm_have &&
                r->asm_have + plen <= r->asm_size) {                /* sigue el objeto partido */
                memcpy(r->obj + r->asm_have, p + cur, plen);
                r->asm_have += (uint32_t)plen;
                if (r->asm_have == r->asm_size) {
                    r->asm_active = 0;
                    r->errors = 0;
                    *data = r->obj;
                    *len = (long)r->asm_size;
                    *pts_ms = r->asm_pts;
                    return 1;
                }
                continue;
            }
            r->asm_active = 0;                                      /* fragmento huérfano: se descarta */
            continue;
        }
bad_packet:
        r->payloads_left = 0;
        if (++r->errors > 5)
            return -1;
    }
}

/* Hora de envío del paquete `index`, o -1. */
static int packet_time(AsfReader *r, uint64_t index, uint32_t *ms)
{
    const AsfInfo *ai = r->ai;
    uint8_t h[64];
    unsigned mult, plt, prop;
    uint32_t pad, len;
    long n = (long)ai->packet_size < 64 ? (long)ai->packet_size : 64;

    if (rd(r->io, ai->data_offset + (long)((unsigned long long)ai->packet_size * index), h, n) != 0)
        return -1;
    if (packet_header(h, (uint32_t)n, ai->packet_size, &mult, &plt, &prop, &pad, ms, &len) < 0)
        return -1;
    return 0;
}

int asf_seek_ms(AsfReader *r, unsigned long ms)
{
    const AsfInfo *ai = r->ai;
    uint64_t lo = 0, hi = ai->num_packets ? ai->num_packets - 1 : 0;
    /*
     * La hora de envío del paquete no incluye el preroll (la de presentación de cada objeto sí): se busca por hora de envío
     * y, como no todos los codificadores la escriben igual, se parte un paquete antes. El salto exacto lo hace quien
     * decodifica, con la hora de presentación del primer objeto que recibe.
     */
    unsigned long target = ms;

    if (ai->num_packets == 0)
        return -1;
    while (lo < hi) {                               /* mayor paquete con hora de envío <= target */
        uint64_t mid = lo + (hi - lo + 1) / 2;
        uint32_t t;

        if (packet_time(r, mid, &t) != 0)
            return -1;
        if (t <= target)
            lo = mid;
        else
            hi = mid - 1;
    }
    if (lo > 0)
        lo--;
    r->next_packet = lo;
    r->payloads_left = 0;
    r->cursor = r->end = 0;
    r->asm_active = 0;
    r->errors = 0;
    return 0;
}
