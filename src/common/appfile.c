/*
 * appfile — ver appfile.h.
 */
#include "appfile.h"

#include <Memory.h>
#include <Processes.h>

#include <string.h>

OSErr appfile_spec(FSSpec *spec)
{
    ProcessSerialNumber psn;
    ProcessInfoRec info;
    OSErr err;

    err = GetCurrentProcess(&psn);
    if (err != noErr)
        return err;

    memset(&info, 0, sizeof info);
    info.processInfoLength = sizeof info;
    info.processName = NULL;
    info.processAppSpec = spec;
    return GetProcessInformation(&psn, &info);
}

OSErr appfile_load_spec(FSSpec *spec, long max_bytes, FileBlob *out)
{
    short ref;
    long size, count;
    OSErr err;

    memset(out, 0, sizeof *out);
    err = FSpOpenDF(spec, fsRdPerm, &ref);
    if (err != noErr)
        return err;
    err = GetEOF(ref, &size);
    if (err != noErr) {
        FSClose(ref);
        return err;
    }
    if (max_bytes > 0 && size > max_bytes)
        size = max_bytes;

    err = memblock_alloc(&out->mem, size);
    if (err != noErr) {
        FSClose(ref);
        return err;
    }
    out->data = out->mem.data;
    out->size = size;

    count = size;
    err = FSRead(ref, &count, out->data);
    FSClose(ref);
    if (err != noErr && err != eofErr) {
        appfile_free(out);
        return err;
    }
    out->size = count;
    return noErr;
}

OSErr appfile_load(const char *name, FileBlob *out)
{
    FSSpec app, spec;
    Str255 pname;
    size_t len = strlen(name);
    OSErr err;

    memset(out, 0, sizeof *out);
    err = appfile_spec(&app);
    if (err != noErr)
        return err;
    if (len > 31)
        len = 31;               /* límite de nombres HFS */
    pname[0] = (unsigned char)len;
    memcpy(pname + 1, name, len);

    err = FSMakeFSSpec(app.vRefNum, app.parID, pname, &spec);
    if (err != noErr)
        return err;
    return appfile_load_spec(&spec, 0, out);
}

void appfile_free(FileBlob *f)
{
    memblock_free(&f->mem);
    memset(f, 0, sizeof *f);
}
