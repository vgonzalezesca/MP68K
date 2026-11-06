/*
 * refcrc — ver refcrc.h.
 */
#include "refcrc.h"
#include "appfile.h"

#include <stdlib.h>
#include <string.h>

static FileBlob sRef;

OSErr refcrc_load(void)
{
    return appfile_load("REFERENCE.txt", &sRef);
}

void refcrc_free(void)
{
    appfile_free(&sRef);
}

int refcrc_find(const char *kind, const char *file, const char *profile,
                unsigned long *pcm_crc, unsigned long *file_crc)
{
    const char *p = (const char *)sRef.data;
    const char *end = p + sRef.size;
    size_t klen = strlen(kind);

    if (p == NULL)
        return 0;
    while (p < end) {
        const char *eol = p;
        char line[200], *f[10];
        long n, i;
        int nf = 0;

        while (eol < end && *eol != '\n' && *eol != '\r')
            eol++;
        n = eol - p;
        if (n > (long)klen && n < (long)sizeof line && strncmp(p, kind, klen) == 0 && p[klen] == ',') {
            memcpy(line, p, n);
            line[n] = '\0';
            f[nf++] = line;
            for (i = 0; i < n && nf < 10; i++)
                if (line[i] == ',') {
                    line[i] = '\0';
                    f[nf++] = line + i + 1;
                }
            if (nf == 10 && strcmp(f[1], file) == 0 && strcmp(f[3], profile) == 0) {
                *file_crc = strtoul(f[2], NULL, 16);
                *pcm_crc = strtoul(f[9], NULL, 16);
                return 1;
            }
        }
        while (eol < end && (*eol == '\n' || *eol == '\r'))
            eol++;
        p = eol;
    }
    return 0;
}
