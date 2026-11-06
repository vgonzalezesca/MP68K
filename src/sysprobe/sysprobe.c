/*
 * sysprobe — Etapa 0 de MP368K.
 *
 * Consulta Gestalt y el Sound Manager y escribe sysprobe.txt junto a la
 * aplicación. No asume nada del equipo: imprime siempre el valor crudo y el
 * código de error; la decodificación es solo orientativa. Lo que importa es
 * la salida de sonido real del LC 580 (¿8 o 16 bits? ¿qué tasas?).
 */
#include <Gestalt.h>
#include <Memory.h>
#include <Sound.h>
#include <Processes.h>
#include <TextUtils.h>
#include <Events.h>

#include <stdio.h>
#include <string.h>

#include "build_info.h"
#include "mactime.h"
#include "sndver.h"
#include "textlog.h"

/*
 * Selectores y constantes que las Multiversal Interfaces no traen. Llevan
 * prefijo MP_ para no chocar con los enums de las Universal Interfaces.
 * Valores tomados de Gestalt.h / Sound.h de Apple: por verificar con la salida.
 */
enum {
    MP_gestaltTimeMgrVersion  = 'tmgr',
    MP_gestaltProcClkSpeed    = 'pclk',
    MP_gestaltComponentMgr    = 'cpnt',
    MP_kMachineNameStrID      = -16395,

    MP_siSampleRate           = 'srat',
    MP_siSampleSize           = 'ssiz',
    MP_siNumberChannels       = 'chan',
    MP_siSampleRateAvailable  = 'srav',
    MP_siSampleSizeAvailable  = 'ssav',
    MP_siChannelAvailable     = 'chav',
    MP_siHardwareFormat       = 'hwfm',
    MP_siHardwareVolume       = 'hvol'
};

/* Bits de gestaltSoundAttr ('snd '). */
static const struct { int bit; const char *name; } kSoundAttrBits[] = {
    {  0, "StereoCapability" },
    {  1, "StereoMixing" },
    {  3, "SoundIOMgrPresent" },
    {  4, "BuiltInSoundInput" },
    {  5, "HasSoundInputDevice" },
    {  6, "PlayAndRecord" },
    {  7, "16BitSoundIO" },
    {  8, "StereoInput" },
    {  9, "LineLevelInput" },
    { 10, "SndPlayDoubleBuffer" },
    { 11, "MultiChannels" },
    { 12, "16BitAudioSupport" },
};

#define MAX_LIST 16

static unsigned be16(const unsigned char *p)
{
    return ((unsigned)p[0] << 8) | p[1];
}

static unsigned long be32(const unsigned char *p)
{
    return ((unsigned long)p[0] << 24) | ((unsigned long)p[1] << 16) |
           ((unsigned long)p[2] << 8) | p[3];
}

static const char *fourcc(unsigned long t, char out[5])
{
    int i;
    for (i = 0; i < 4; i++) {
        char c = (char)(t >> (24 - 8 * i));
        out[i] = (c >= 32 && c < 127) ? c : '.';
    }
    out[4] = '\0';
    return out;
}

/* UnsignedFixed 16.16 -> "entero.mmm" sin punto flotante. */
static const char *ufixed(unsigned long v, char out[16])
{
    unsigned long frac = ((v & 0xFFFFUL) * 1000UL + 0x8000UL) >> 16;
    unsigned long whole = v >> 16;
    if (frac >= 1000) {
        frac -= 1000;
        whole++;
    }
    snprintf(out, 16, "%lu.%03lu", whole, frac);
    return out;
}

/* Consulta Gestalt e imprime el resultado. Devuelve true si no hubo error. */
static Boolean gestalt(const char *label, unsigned long sel, long *value)
{
    char cc[5];
    long resp = 0;
    OSErr err = Gestalt(sel, (LONGINT *)&resp);

    if (err != noErr)
        textlog_printf("%-20s '%s' err=%d\n", label, fourcc(sel, cc), err);
    else
        textlog_printf("%-20s '%s' = %ld (0x%08lX)\n", label, fourcc(sel, cc),
                       resp, (unsigned long)resp);
    if (value)
        *value = resp;
    return err == noErr;
}

static void probe_gestalt(void)
{
    static const char *const kProc[] = { "?", "68000", "68010", "68020", "68030", "68040" };
    static const char *const kFpu[] = { "ninguna", "68881", "68882", "integrada 68040" };
    static const char *const kMmu[] = { "ninguna", "AMU", "68851", "68030", "68040" };
    long v;
    unsigned i;

    textlog_printf("\n[gestalt]\n");

    if (gestalt("machine", gestaltMachineType, &v)) {
        Str255 name;
        name[0] = 0;
        GetIndString(name, MP_kMachineNameStrID, (short)v);
        textlog_printf("  nombre: \"%.*s\" (LC 580 real: esperado 99)\n", name[0], name + 1);
    }
    if (gestalt("system", gestaltSystemVersion, &v))
        textlog_printf("  version: %lX.%lX.%lX\n", (v >> 8) & 0xFF, (v >> 4) & 0xF, v & 0xF);
    if (gestalt("processor", gestaltProcessorType, &v))
        textlog_printf("  cpu: %s\n", (v >= 0 && v <= 5) ? kProc[v] : "desconocida");
    if (gestalt("fpu", gestaltFPUType, &v))
        textlog_printf("  fpu: %s\n", (v >= 0 && v <= 3) ? kFpu[v] : "desconocida");
    if (gestalt("mmu", gestaltMMUType, &v))
        textlog_printf("  mmu: %s\n", (v >= 0 && v <= 4) ? kMmu[v] : "desconocida");
    /* En un PowerPC, "processor" informa el 68k emulado; la CPU real sale de aquí. */
    if (gestalt("native_cpu", gestaltNativeCPUtype, &v)) {
        /* Códigos de Multiverse.h (gestaltCPU...). */
        const char *name = v == 0x004 ? "68040" : v == 0x101 ? "PowerPC 601" :
                           v == 0x103 ? "PowerPC 603" : v == 0x104 ? "PowerPC 604" :
                           v == 0x106 ? "PowerPC 603e" : v == 0x107 ? "PowerPC 603ev" :
                           v == 0x108 ? "PowerPC 750 (G3)" : v == 0x109 ? "PowerPC 604e" :
                           v == 0x10A ? "PowerPC 604ev" : v == 0x10C ? "PowerPC G4" :
                           "desconocida";
        textlog_printf("  cpu nativa: %s (0x%03lX)\n", name, v);
    }
    gestalt("sys_arch", gestaltSysArchitecture, NULL);
    gestalt("proc_clock_hz", MP_gestaltProcClkSpeed, NULL);
    if (gestalt("phys_ram", gestaltPhysicalRAMSize, &v))
        textlog_printf("  %ld KB\n", v / 1024);
    if (gestalt("logical_ram", gestaltLogicalRAMSize, &v))
        textlog_printf("  %ld KB\n", v / 1024);
    if (gestalt("vm", gestaltVMAttr, &v))
        textlog_printf("  memoria virtual: %s\n", (v & 1) ? "activa" : "inactiva");
    gestalt("rom_size", gestaltROMSize, NULL);
    gestalt("rom_version", gestaltROMVersion, NULL);
    gestalt("hardware_attr", gestaltHardwareAttr, NULL);
    gestalt("quickdraw", gestaltQuickdrawVersion, NULL);
    if (gestalt("time_mgr", MP_gestaltTimeMgrVersion, &v))
        textlog_printf("  Microseconds disponible: %s\n", v >= 3 ? "si" : "no");
    gestalt("component_mgr", MP_gestaltComponentMgr, NULL);

    if (gestalt("sound_attr", gestaltSoundAttr, &v)) {
        for (i = 0; i < sizeof kSoundAttrBits / sizeof kSoundAttrBits[0]; i++)
            textlog_printf("  bit %2d %-20s %s\n", kSoundAttrBits[i].bit,
                           kSoundAttrBits[i].name,
                           (v >> kSoundAttrBits[i].bit) & 1 ? "SI" : "no");
    }
}

/*
 * Lee una lista {short n; Handle h} devuelta por SndGetInfo. Copia hasta
 * MAX_LIST elementos de `elemSize` bytes y libera el handle. Devuelve el
 * número de elementos copiados; *declared recibe n.
 */
static int read_info_list(const unsigned char *buf, int elemSize,
                          unsigned long out[MAX_LIST], int *declared)
{
    Handle h;
    long bytes;
    int count, i;

    *declared = (short)be16(buf);
    memcpy(&h, buf + 2, sizeof h);
    if (h == NULL)
        return 0;

    bytes = GetHandleSize(h);
    count = (int)(bytes / elemSize);
    if (count > MAX_LIST)
        count = MAX_LIST;

    HLock(h);
    for (i = 0; i < count; i++) {
        const unsigned char *p = (const unsigned char *)*h + i * elemSize;
        out[i] = elemSize == 4 ? be32(p) : be16(p);
    }
    HUnlock(h);
    DisposeHandle(h);
    return count;
}

static OSErr snd_info(SndChannelPtr chan, unsigned long sel, unsigned char buf[64])
{
    char cc[5];
    OSErr err;

    memset(buf, 0, 64);
    err = SndGetInfo(chan, (OSType)sel, buf);
    textlog_printf("%-20s '%s' err=%d", "SndGetInfo", fourcc(sel, cc), err);
    return err;
}

static void probe_output(SndChannelPtr chan)
{
    /* Alineado a 4 bytes; 64 bytes de margen por si un selector escribe más. */
    union { unsigned char b[64]; long align; } u;
    unsigned char *buf = u.b;
    unsigned long list[MAX_LIST];
    char s[16], cc[5];
    int n, declared, i;

    if (snd_info(chan, MP_siSampleRate, buf) == noErr)
        textlog_printf("  tasa actual: %s Hz", ufixed(be32(buf), s));
    textlog_printf("\n");

    if (snd_info(chan, MP_siSampleSize, buf) == noErr)
        textlog_printf("  bits actuales: %u", be16(buf));
    textlog_printf("\n");

    if (snd_info(chan, MP_siNumberChannels, buf) == noErr)
        textlog_printf("  canales actuales: %u", be16(buf));
    textlog_printf("\n");

    if (snd_info(chan, MP_siChannelAvailable, buf) == noErr)
        textlog_printf("  canales disponibles: %u", be16(buf));
    textlog_printf("\n");

    if (snd_info(chan, MP_siSampleRateAvailable, buf) == noErr) {
        n = read_info_list(buf, 4, list, &declared);
        textlog_printf("  tasas declaradas: %d%s\n", declared,
                       declared == 0 ? " (0 = rango continuo min..max)" : "");
        for (i = 0; i < n; i++)
            textlog_printf("    %s Hz\n", ufixed(list[i], s));
    } else
        textlog_printf("\n");

    if (snd_info(chan, MP_siSampleSizeAvailable, buf) == noErr) {
        n = read_info_list(buf, 2, list, &declared);
        textlog_printf("  tamanos declarados: %d\n", declared);
        for (i = 0; i < n; i++)
            textlog_printf("    %lu bits\n", list[i]);
    } else
        textlog_printf("\n");

    /* SoundComponentData: flags, format, numChannels, sampleSize, sampleRate, ... */
    if (snd_info(chan, MP_siHardwareFormat, buf) == noErr)
        textlog_printf("  hw: formato '%s' canales %u bits %u tasa %s Hz",
                       fourcc(be32(buf + 4), cc), be16(buf + 8), be16(buf + 10),
                       ufixed(be32(buf + 12), s));
    textlog_printf("\n");

    if (snd_info(chan, MP_siHardwareVolume, buf) == noErr)
        textlog_printf("  volumen hw: 0x%08lX", be32(buf));
    textlog_printf("\n");
}

static void probe_sound(void)
{
    uint32_t nv;
    unsigned char v[4];
    unsigned char st[6];
    SndChannelPtr chan = NULL;
    OSErr err;

    textlog_printf("\n[sound]\n");

    /* Ver sndver.h: en PowerPC no se puede llamar a la función de versión directamente. */
    nv = sndver_get();
    v[0] = (unsigned char)(nv >> 24);
    v[1] = (unsigned char)(nv >> 16);
    v[2] = (unsigned char)(nv >> 8);
    v[3] = (unsigned char)nv;
    textlog_printf("sound_mgr_version    %u.%u.%u stage 0x%02X rev %u\n",
                   v[0], v[1] >> 4, v[1] & 0xF, v[2], v[3]);

    memset(st, 0, sizeof st);
    err = SndManagerStatus(sizeof st, (SMStatusPtr)st);
    textlog_printf("SndManagerStatus     err=%d", err);
    if (err == noErr)
        textlog_printf(" maxCPULoad=%u numChannels=%u curCPULoad=%u",
                       be16(st), be16(st + 2), be16(st + 4));
    textlog_printf("\n");

    if (v[0] < 3) {
        textlog_printf("Sound Manager < 3.0: SndGetInfo no disponible, se omite.\n");
        return;
    }

    err = SndNewChannel(&chan, sampledSynth, 0, NULL);
    textlog_printf("SndNewChannel        err=%d\n", err);
    if (err != noErr || chan == NULL)
        return;

    probe_output(chan);

    err = SndDisposeChannel(chan, true);
    textlog_printf("SndDisposeChannel    err=%d\n", err);
}

static void probe_memory(void)
{
    ProcessSerialNumber psn;
    ProcessInfoRec info;

    textlog_printf("\n[memory]\n");
    textlog_printf("FreeMem              %ld\n", (long)FreeMem());
    textlog_printf("MaxBlock             %ld\n", (long)MaxBlock());
    textlog_printf("TempFreeMem          %ld\n", (long)TempFreeMem());

    memset(&info, 0, sizeof info);
    info.processInfoLength = sizeof info;
    if (GetCurrentProcess(&psn) == noErr && GetProcessInformation(&psn, &info) == noErr)
        textlog_printf("particion_app        %lu\n", (unsigned long)info.processSize);
}

/* Compara Microseconds contra 60 ticks (~0,9975 s si el tick es de 60,15 Hz). */
static void probe_clock(void)
{
    unsigned long t0, t;
    uint64_t u0, u1;

    textlog_printf("\n[clock]\n");
    t0 = TickCount();
    while ((t = TickCount()) == t0)
        ;
    u0 = mactime_usec();
    while (TickCount() - t < 60)
        ;
    u1 = mactime_usec();
    textlog_printf("usec_por_60_ticks    %lu (esperado ~997500)\n", (unsigned long)(u1 - u0));
}

int main(void)
{
    OSErr err;

    textlog_init();
    textlog_printf("# MP368K sysprobe\n");
    textlog_printf("build: %s %s %s %s\n", MP368K_BUILD_ID, __DATE__, __TIME__,
                   MP368K_BUILD_FLAGS);

    probe_gestalt();
    probe_sound();
    probe_memory();
    probe_clock();

    textlog_printf("\n[cache]\n");
    textlog_printf("no consultable desde la app (CACR es privilegiado); anotar a mano\n");

    err = textlog_save_next_to_app("sysprobe.txt");
    textlog_printf("\nsysprobe.txt guardado: err=%d%s\n", err,
                   textlog_truncated() ? " (TRUNCADO)" : "");
    printf("\n(Presione Return para salir)\n");
    getchar();
    return 0;
}
