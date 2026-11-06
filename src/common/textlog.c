/*
 * textlog — ver textlog.h.
 */
#include "textlog.h"
#include "appfile.h"

#include <Files.h>
#if __has_include(<Script.h>)
#include <Script.h>     /* Universal Interfaces; Multiversal lo trae en Multiverse.h */
#endif

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* 16 KB alcanzan de sobra para sysprobe; el banco de pruebas puede subirlo. */
#define TEXTLOG_CAPACITY 16384

static char sBuffer[TEXTLOG_CAPACITY];
static long sLength;
static Boolean sTruncated;

void textlog_init(void)
{
    sLength = 0;
    sTruncated = false;
    sBuffer[0] = '\0';
}

void textlog_printf(const char *fmt, ...)
{
    char line[512];
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    if (n < 0)
        return;
    if (n >= (int)sizeof line) {
        n = sizeof line - 1;
        sTruncated = true;
    }

    fputs(line, stdout);

    if (sLength + n >= TEXTLOG_CAPACITY) {
        n = TEXTLOG_CAPACITY - 1 - sLength;
        sTruncated = true;
    }
    if (n > 0) {
        memcpy(sBuffer + sLength, line, n);
        sLength += n;
        sBuffer[sLength] = '\0';
    }
}

Boolean textlog_truncated(void)
{
    return sTruncated;
}

OSErr textlog_save_next_to_app(const char *name)
{
    FSSpec app, out;
    Str255 pname;
    size_t len;
    short ref;
    long count, i;
    OSErr err;

    err = appfile_spec(&app);
    if (err != noErr)
        return err;

    len = strlen(name);
    if (len > 31)
        len = 31;               /* límite de nombres HFS */
    pname[0] = (unsigned char)len;
    memcpy(pname + 1, name, len);

    err = FSMakeFSSpec(app.vRefNum, app.parID, pname, &out);
    if (err == noErr)
        FSpDelete(&out);
    else if (err != fnfErr)
        return err;

    err = FSpCreate(&out, 'ttxt', 'TEXT', smSystemScript);
    if (err != noErr)
        return err;
    err = FSpOpenDF(&out, fsWrPerm, &ref);
    if (err != noErr)
        return err;

    /* Fin de línea de Mac OS clásico. En el host: tr '\r' '\n'. */
    for (i = 0; i < sLength; i++)
        if (sBuffer[i] == '\n')
            sBuffer[i] = '\r';

    count = sLength;
    err = FSWrite(ref, &count, sBuffer);

    for (i = 0; i < sLength; i++)
        if (sBuffer[i] == '\r')
            sBuffer[i] = '\n';

    if (err == noErr)
        err = SetEOF(ref, count);
    FSClose(ref);
    FlushVol(NULL, out.vRefNum);
    return err;
}
