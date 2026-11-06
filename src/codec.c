/*
 * codec — ver codec.h.
 */
#include "codec.h"

#include <string.h>

/* El MP3 va al final: su reconocimiento es el más débil (acepta casi cualquier cosa). */
static const Codec *const kCodecs[] = { &codec_flac, &codec_mp3 };
#define CODEC_COUNT (sizeof kCodecs / sizeof kCodecs[0])

static Arena sArena;

void codec_init(void *arena_mem, size_t size)
{
    arena_init(&sArena, arena_mem, size);
}

Arena *codec_arena(void)
{
    return &sArena;
}

static int read_at(const CodecIO *io, long offset, uint8_t *dst, long want, long *got)
{
    long total = 0, n;

    if (io->seek(io->ctx, offset) != 0)
        return -1;
    while (total < want) {
        n = 0;
        if (io->read(io->ctx, dst + total, want - total, &n) != 0)
            return -1;
        if (n <= 0)
            break;
        total += n;
    }
    *got = total;
    return 0;
}

const Codec *codec_probe_any(CodecIO *io, uint8_t *buf, long cap, CodecInfo *ci, int *err)
{
    uint8_t head[16];
    long got = 0, start = 0;
    size_t i;

    *err = CODEC_ERR_UNSUPPORTED;
    io->audio_start = 0;
    if (read_at(io, 0, head, sizeof head, &got) != 0) {
        *err = CODEC_ERR_IO;
        return NULL;
    }
    if (got >= 10) {
        start = id3_v2_total_size(head, (size_t)got);
        if (start > 0) {
            if (start >= io->size) {
                return NULL;
            }
            if (read_at(io, start, head, sizeof head, &got) != 0) {
                *err = CODEC_ERR_IO;
                return NULL;
            }
            io->audio_start = start;
        }
    }
    for (i = 0; i < CODEC_COUNT; i++) {
        if (kCodecs[i]->sniff(head, (size_t)got)) {
            memset(ci, 0, sizeof *ci);
            *err = kCodecs[i]->probe(io, buf, cap, ci);
            return *err == CODEC_OK ? kCodecs[i] : NULL;
        }
    }
    return NULL;
}

static int ends_with_ci(const char *s, const char *suffix)
{
    size_t n = strlen(s), m = strlen(suffix), i;

    if (n < m)
        return 0;
    for (i = 0; i < m; i++) {
        char c = s[n - m + i];

        if (c >= 'A' && c <= 'Z')
            c = (char)(c - 'A' + 'a');
        if (c != suffix[i])
            return 0;
    }
    return 1;
}

int codec_name_matches(const char *name, unsigned long mac_type)
{
    static const char *const kExt[] = { ".mp3", ".flac" };
    static const unsigned long kTypes[] = {
        0x4D504733UL /* MPG3 */, 0x4D503320UL /* MP3  */, 0x4D703320UL /* Mp3  */, 0x6D703321UL /* mp3! */,
        0x464C4143UL /* FLAC */, 0x664C6143UL /* fLaC */
    };
    size_t i;

    for (i = 0; i < sizeof kExt / sizeof kExt[0]; i++)
        if (ends_with_ci(name, kExt[i]))
            return 1;
    for (i = 0; i < sizeof kTypes / sizeof kTypes[0]; i++)
        if (mac_type == kTypes[i])
            return 1;
    return 0;
}
