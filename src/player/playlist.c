/*
 * playlist — ver playlist.h.
 */
#include "playlist.h"

#include <Files.h>
#include <Memory.h>

#include <string.h>

#include "appfile.h"
#include "fileread.h"
#include "id3.h"
#include "memblock.h"
#include "qtfmt.h"
#include "qtplay.h"
#include "trackinfo.h"

#define M3U_MAX_BYTES (64L * 1024)
#define NAME_CAP 64                 /* bytes por nombre al ordenar una carpeta */

static MemBlock sMem;
static PlaylistEntry *sEntries;
static unsigned sCount;
static Order sOrder;
static uint8_t sProbe[8192];        /* memoria de trabajo para leer cabeceras */

OSErr playlist_init(void)
{
    OSErr err = memblock_alloc(&sMem, (long)PLAYLIST_MAX * (long)sizeof(PlaylistEntry));

    if (err != noErr)
        return err;
    sEntries = (PlaylistEntry *)sMem.data;
    sCount = 0;
    order_init(&sOrder, 0x2545F491UL ^ (unsigned long)TickCount());
    return noErr;
}

void playlist_shutdown(void)
{
    memblock_free(&sMem);
    sEntries = NULL;
    sCount = 0;
}

Order *playlist_order(void)
{
    return &sOrder;
}

unsigned playlist_count(void)
{
    return sCount;
}

const PlaylistEntry *playlist_get(unsigned index)
{
    return index < sCount ? &sEntries[index] : NULL;
}

static void pstr_to_c(const unsigned char *p, char *out, size_t cap)
{
    size_t n = p[0];

    if (n >= cap)
        n = cap - 1;
    memcpy(out, p + 1, n);
    out[n] = '\0';
}

static int add_entry(FSSpec *spec)
{
    FileReader fr;
    CodecIO io;
    CodecInfo ci;
    const Codec *codec;
    Boolean via_qt = false;
    PlaylistEntry *e;
    char fname[64];
    OSErr err;
    int idx;

    if (sCount >= PLAYLIST_MAX || fileread_open_spec(&fr, spec) != noErr)
        return -1;
    err = trackinfo_probe_ex(&fr, &io, sProbe, sizeof sProbe, &ci, &codec, &via_qt);
    fileread_close(&fr);
    if (err != noErr)
        return -1;
    idx = order_add(&sOrder);
    if (idx < 0)
        return -1;
    e = &sEntries[idx];
    e->spec = *spec;
    pstr_to_c(spec->name, fname, sizeof fname);
    trackinfo_display_name(fname, &ci.tags, e->label, sizeof e->label);
    e->duration_ms = ci.duration_ms;
    sCount = (unsigned)idx + 1;
    return idx;
}

int playlist_add_file(FSSpec *spec)
{
    return add_entry(spec);
}

void playlist_remove(unsigned index)
{
    if (index >= sCount)
        return;
    memmove(&sEntries[index], &sEntries[index + 1], (sCount - index - 1) * sizeof sEntries[0]);
    sCount--;
    order_remove(&sOrder, index);
}

void playlist_clear(void)
{
    sCount = 0;
    order_clear(&sOrder);
}

/* ------------------------------------------------------------------------ */
/* Carpeta                                                                   */
/* ------------------------------------------------------------------------ */

static unsigned char lower(unsigned char c)
{
    return (c >= 'A' && c <= 'Z') ? (unsigned char)(c + 32) : c;
}

static int name_cmp(const unsigned char *a, const unsigned char *b)
{
    unsigned i, n = a[0] < b[0] ? a[0] : b[0];

    for (i = 1; i <= n; i++)
        if (lower(a[i]) != lower(b[i]))
            return lower(a[i]) < lower(b[i]) ? -1 : 1;
    return (int)a[0] - (int)b[0];
}

static Boolean ends_with(const unsigned char *pname, const char *ext)
{
    size_t n = strlen(ext), i;

    if (pname[0] < n)
        return false;
    for (i = 0; i < n; i++)
        if (lower(pname[pname[0] - n + 1 + i]) != (unsigned char)ext[i])
            return false;
    return true;
}

Boolean playlist_is_m3u(const unsigned char *pname)
{
    return ends_with(pname, ".m3u") || ends_with(pname, ".m3u8");
}

/* ¿El nombre (cadena Pascal) o el tipo de archivo sugieren un formato que sabemos abrir? */
static Boolean looks_like_audio(const unsigned char *pname, OSType type)
{
    char cname[64];

    pstr_to_c(pname, cname, sizeof cname);
    return codec_name_matches(cname, (unsigned long)type) != 0 ||
           (qtplay_available() && qtfmt_name_matches(cname, (unsigned long)type) != 0);
}

unsigned playlist_add_folder_of(FSSpec *any_file, unsigned *skipped)
{
    MemBlock names;
    unsigned char (*list)[NAME_CAP];
    unsigned n = 0, i, j, added = 0, bad = 0;
    long idx;

    if (skipped != NULL)
        *skipped = 0;
    if (memblock_alloc(&names, (long)PLAYLIST_MAX * NAME_CAP) != noErr)
        return 0;
    list = (unsigned char (*)[NAME_CAP])names.data;

    for (idx = 1; idx < 4000 && n < PLAYLIST_MAX; idx++) {
        CInfoPBRec pb;
        Str255 nm;

        memset(&pb, 0, sizeof pb);
        nm[0] = 0;
        pb.hFileInfo.ioNamePtr = nm;
        pb.hFileInfo.ioVRefNum = any_file->vRefNum;
        pb.hFileInfo.ioDirID = any_file->parID;
        pb.hFileInfo.ioFDirIndex = (short)idx;
        if (PBGetCatInfoSync(&pb) != noErr)
            break;                                          /* fin de la carpeta */
        if (pb.hFileInfo.ioFlAttrib & 0x10)
            continue;                                       /* subcarpeta */
        if (!looks_like_audio(nm, pb.hFileInfo.ioFlFndrInfo.fdType) || nm[0] >= NAME_CAP)
            continue;
        memcpy(list[n], nm, (size_t)nm[0] + 1);
        n++;
    }

    for (i = 1; i < n; i++) {                               /* ordenar por nombre (inserción) */
        unsigned char tmp[NAME_CAP];

        memcpy(tmp, list[i], NAME_CAP);
        for (j = i; j > 0 && name_cmp(list[j - 1], tmp) > 0; j--)
            memcpy(list[j], list[j - 1], NAME_CAP);
        memcpy(list[j], tmp, NAME_CAP);
    }

    for (i = 0; i < n; i++) {
        FSSpec spec;

        if (FSMakeFSSpec(any_file->vRefNum, any_file->parID, list[i], &spec) == noErr && add_entry(&spec) >= 0)
            added++;
        else
            bad++;
    }
    memblock_free(&names);
    if (skipped != NULL)
        *skipped = bad;
    return added;
}

/* ------------------------------------------------------------------------ */
/* .m3u                                                                      */
/* ------------------------------------------------------------------------ */

/* Convierte UTF-8 a MacRoman. Devuelve verdadero si había secuencias multibyte. */
static Boolean utf8_to_macroman(const char *src, char *dst, size_t cap)
{
    size_t o = 0;
    Boolean multi = false;

    while (*src && o + 1 < cap) {
        unsigned char c = (unsigned char)*src++;
        unsigned long cp;
        int extra = 0;

        if (c < 0x80) {
            dst[o++] = (char)c;
            continue;
        }
        if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; extra = 1; }
        else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; extra = 2; }
        else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; extra = 3; }
        else { dst[o++] = '?'; continue; }
        while (extra-- > 0 && ((unsigned char)*src & 0xC0) == 0x80) {
            cp = (cp << 6) | ((unsigned char)*src++ & 0x3F);
            multi = true;
        }
        dst[o++] = (char)id3_cp_to_macroman(cp);
    }
    dst[o] = '\0';
    return multi;
}

static void latin1_to_macroman(const char *src, char *dst, size_t cap)
{
    size_t o = 0;

    while (*src && o + 1 < cap)
        dst[o++] = (char)id3_latin1_to_macroman((unsigned char)*src++);
    dst[o] = '\0';
}

/*
 * Ruta de la lista (con '/' o '\') -> ruta relativa de HFS en cadena Pascal: "a/b.mp3" -> ":a:b.mp3",
 * "../b.mp3" -> "::b.mp3". Las rutas absolutas o con unidad (C:\...) se reducen al nombre del archivo.
 */
static Boolean build_mac_path(const char *line, unsigned char *out)
{
    char tmp[300];
    const char *p = tmp;
    char *q;
    size_t o, n = strlen(line), len;

    if (n >= sizeof tmp)
        return false;
    memcpy(tmp, line, n + 1);
    for (q = tmp; *q; q++)
        if (*q == '\\')
            *q = '/';
    if (!strncmp(tmp, "file://", 7))
        p = tmp + 7, n = strlen(p);
    if (p[0] == '/' || (((p[0] >= 'A' && p[0] <= 'Z') || (p[0] >= 'a' && p[0] <= 'z')) && p[1] == ':')) {
        const char *slash = strrchr(p, '/');                /* absoluta: solo el nombre */

        p = slash != NULL ? slash + 1 : p;
    }

    out[1] = ':';
    o = 2;
    while (*p) {
        const char *end = strchr(p, '/');
        size_t clen = end != NULL ? (size_t)(end - p) : strlen(p);

        if (clen == 0 || (clen == 1 && p[0] == '.')) {
            /* componente vacío o "." : se ignora */
        } else if (clen == 2 && p[0] == '.' && p[1] == '.') {
            if (o + 1 >= 255)
                return false;
            out[o++] = ':';                                 /* sube una carpeta */
        } else {
            size_t k;

            if (o + clen + 1 >= 255)
                return false;
            for (k = 0; k < clen; k++)
                out[o++] = (unsigned char)(p[k] == ':' ? '-' : p[k]);
            if (end != NULL)
                out[o++] = ':';
        }
        if (end == NULL)
            break;
        p = end + 1;
    }
    len = o - 1;
    out[0] = (unsigned char)len;
    return len > 1;
}

static Boolean has_high_bit(const char *s)
{
    for (; *s; s++)
        if ((unsigned char)*s >= 0x80)
            return true;
    return false;
}

static Boolean resolve_line(const FSSpec *base, const char *line, FSSpec *out)
{
    unsigned char path[256];
    char conv[300];

    if (build_mac_path(line, path) && FSMakeFSSpec(base->vRefNum, base->parID, path, out) == noErr)
        return true;
    if (!has_high_bit(line))
        return false;
    /* Nombres con acentos: probar como UTF-8 y como Latin-1/Windows. */
    if (utf8_to_macroman(line, conv, sizeof conv) && build_mac_path(conv, path) &&
        FSMakeFSSpec(base->vRefNum, base->parID, path, out) == noErr)
        return true;
    latin1_to_macroman(line, conv, sizeof conv);
    return build_mac_path(conv, path) && FSMakeFSSpec(base->vRefNum, base->parID, path, out) == noErr;
}

unsigned playlist_add_m3u(FSSpec *m3u, unsigned *skipped)
{
    FileBlob fb;
    const char *p, *end;
    unsigned added = 0, bad = 0;

    if (skipped != NULL)
        *skipped = 0;
    if (appfile_load_spec(m3u, M3U_MAX_BYTES, &fb) != noErr)
        return 0;
    p = (const char *)fb.data;
    end = p + fb.size;
    if (fb.size >= 3 && (unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB && (unsigned char)p[2] == 0xBF)
        p += 3;                                             /* BOM de UTF-8 */

    while (p < end) {
        const char *eol = p;
        char line[300];
        size_t n, a;
        FSSpec spec;

        while (eol < end && *eol != '\n' && *eol != '\r')
            eol++;
        n = (size_t)(eol - p);
        if (n >= sizeof line)
            n = sizeof line - 1;
        memcpy(line, p, n);
        line[n] = '\0';
        for (a = 0; line[a] == ' ' || line[a] == '\t'; a++)
            ;
        while (n > a && (line[n - 1] == ' ' || line[n - 1] == '\t'))
            line[--n] = '\0';
        if (line[a] != '\0' && line[a] != '#') {
            if (resolve_line(m3u, line + a, &spec) && add_entry(&spec) >= 0)
                added++;
            else
                bad++;
        }
        while (eol < end && (*eol == '\n' || *eol == '\r'))
            eol++;
        p = eol;
    }
    appfile_free(&fb);
    if (skipped != NULL)
        *skipped = bad;
    return added;
}
