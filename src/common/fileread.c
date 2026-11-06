/*
 * fileread — ver fileread.h.
 */
#include "fileread.h"
#include "appfile.h"

#include <string.h>

OSErr fileread_open_spec(FileReader *f, FSSpec *spec)
{
    OSErr err;

    f->ref = 0;
    f->size = 0;
    err = FSpOpenDF(spec, fsRdPerm, &f->ref);
    if (err != noErr) {
        f->ref = 0;
        return err;
    }
    err = GetEOF(f->ref, &f->size);
    if (err != noErr) {
        FSClose(f->ref);
        f->ref = 0;
    }
    return err;
}

OSErr fileread_open_app_file(FileReader *f, const char *name)
{
    FSSpec app, spec;
    Str255 pname;
    size_t len = strlen(name);
    OSErr err;

    err = appfile_spec(&app);
    if (err != noErr)
        return err;
    if (len > 31)
        len = 31;
    pname[0] = (unsigned char)len;
    memcpy(pname + 1, name, len);
    err = FSMakeFSSpec(app.vRefNum, app.parID, pname, &spec);
    if (err != noErr)
        return err;
    return fileread_open_spec(f, &spec);
}

void fileread_close(FileReader *f)
{
    if (f->ref != 0) {
        FSClose(f->ref);
        f->ref = 0;
    }
}

int fileread_read(void *ctx, uint8_t *dst, long max, long *got)
{
    FileReader *f = ctx;
    long count = max;
    OSErr err = FSRead(f->ref, &count, dst);

    if (err != noErr && err != eofErr)
        return -1;
    *got = count;               /* 0 = fin de archivo */
    return 0;
}

int fileread_seek(void *ctx, long offset)
{
    FileReader *f = ctx;

    return SetFPos(f->ref, fsFromStart, offset) == noErr ? 0 : -1;
}
