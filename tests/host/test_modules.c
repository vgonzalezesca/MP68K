/*
 * test_modules — pruebas del PC para id3 (etiquetas armadas a mano) y order.
 */
#include "id3.h"
#include "order.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
#define CHECK(cond, ...) do { if (!(cond)) { failures++; printf("FALLA: "); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* ---- armado de etiquetas ---- */
typedef struct { uint8_t b[1024]; size_t n; } Buf;

static void put(Buf *x, const void *p, size_t n) { memcpy(x->b + x->n, p, n); x->n += n; }
static void put1(Buf *x, unsigned v) { x->b[x->n++] = (uint8_t)v; }

static void synchsafe(uint8_t *d, unsigned long v)
{
    d[0] = (v >> 21) & 0x7F; d[1] = (v >> 14) & 0x7F; d[2] = (v >> 7) & 0x7F; d[3] = v & 0x7F;
}

/* Frame de texto. ver: 2, 3 o 4. flags solo para 3 y 4. */
static void frame(Buf *x, int ver, const char *id, unsigned flags, const void *data, size_t n)
{
    uint8_t h[10];

    if (ver == 2) {
        memcpy(h, id, 3);
        h[3] = (n >> 16) & 0xFF; h[4] = (n >> 8) & 0xFF; h[5] = n & 0xFF;
        put(x, h, 6);
    } else {
        memcpy(h, id, 4);
        if (ver == 4) synchsafe(h + 4, n);
        else { h[4] = n >> 24; h[5] = n >> 16; h[6] = n >> 8; h[7] = n; }
        h[8] = flags >> 8; h[9] = flags & 0xFF;
        put(x, h, 10);
    }
    put(x, data, n);
}

static void begin_tag(Buf *x, int ver, unsigned flags)
{
    uint8_t h[10] = { 'I', 'D', '3', 0, 0, 0, 0, 0, 0, 0 };

    h[3] = (uint8_t)ver;
    h[5] = (uint8_t)flags;
    x->n = 0;
    put(x, h, 10);
}

static void end_tag(Buf *x, size_t padding)
{
    memset(x->b + x->n, 0, padding);
    x->n += padding;
    synchsafe(x->b + 6, x->n - 10);
}

static void test_id3(void)
{
    Buf t;
    Id3Tags tg;
    long total;

    /* ID3v2.3, Latin-1 y UTF-16LE con BOM */
    begin_tag(&t, 3, 0);
    frame(&t, 3, "TIT2", 0, "\x00" "Canci\xF3n de prueba", 18);
    frame(&t, 3, "TPE1", 0, "\x00\xD1" "and\xFA", 6);
    frame(&t, 3, "TALB", 0, "\x01\xFF\xFE\xC1\x00" "l\0b\0u\0m\0", 13);
    end_tag(&t, 16);
    memset(&tg, 0, sizeof tg);
    total = id3_parse_v2(t.b, t.n, &tg);
    CHECK(total == (long)t.n, "v2.3: tamano %ld != %zu", total, t.n);
    CHECK(!strcmp(tg.title, "Canci\x97n de prueba"), "v2.3 titulo: '%s'", tg.title);
    CHECK(!strcmp(tg.artist, "\x84" "and\x9C"), "v2.3 artista: '%s'", tg.artist);
    CHECK(!strcmp(tg.album, "\xE7" "lbum"), "v2.3 album UTF-16LE: '%s'", tg.album);

    /* ID3v2.4: UTF-8 (con raya larga) y UTF-16BE sin BOM */
    begin_tag(&t, 4, 0);
    frame(&t, 4, "TIT2", 0, "\x03" "Caf\xC3\xA9 \xE2\x80\x94 d\xC3\xA9j\xC3\xA0", 19);
    frame(&t, 4, "TPE1", 0, "\x02\x00" "B\0j\0\xF6\0r\0k", 11);
    end_tag(&t, 8);
    memset(&tg, 0, sizeof tg);
    id3_parse_v2(t.b, t.n, &tg);
    CHECK(!strcmp(tg.title, "Caf\x8E \xD1 d\x8Ej\x88"), "v2.4 titulo UTF-8: '%s'", tg.title);
    CHECK(!strcmp(tg.artist, "Bj\x9Ark"), "v2.4 artista UTF-16BE: '%s'", tg.artist);

    /* ID3v2.4 con desincronizacion en el frame: "ÿ" (FF) viaja como FF 00 */
    begin_tag(&t, 4, 0);
    frame(&t, 4, "TIT2", 0x0002, "\x00\xFF\x00" "x", 4);
    end_tag(&t, 4);
    memset(&tg, 0, sizeof tg);
    id3_parse_v2(t.b, t.n, &tg);
    CHECK(!strcmp(tg.title, "\xD8" "x"), "v2.4 desincronizado: '%s'", tg.title);

    /* ID3v2.2 */
    begin_tag(&t, 2, 0);
    frame(&t, 2, "TT2", 0, "\x00Title", 6);
    frame(&t, 2, "TP1", 0, "\x00" "Art\xE9", 5);
    end_tag(&t, 8);
    memset(&tg, 0, sizeof tg);
    id3_parse_v2(t.b, t.n, &tg);
    CHECK(!strcmp(tg.title, "Title") && !strcmp(tg.artist, "Art\x8E"), "v2.2: '%s' / '%s'", tg.title, tg.artist);

    /* cabecera extendida en v2.3 (6 bytes) antes de los frames */
    begin_tag(&t, 3, 0x40);
    put(&t, "\x00\x00\x00\x06\x00\x00\x00\x00\x00\x00", 10);
    frame(&t, 3, "TIT2", 0, "\x00" "Despues", 8);
    end_tag(&t, 4);
    memset(&tg, 0, sizeof tg);
    id3_parse_v2(t.b, t.n, &tg);
    CHECK(!strcmp(tg.title, "Despues"), "cabecera extendida: '%s'", tg.title);

    /* etiqueta truncada: se queda con lo que alcance y devuelve el tamano total */
    begin_tag(&t, 3, 0);
    frame(&t, 3, "TIT2", 0, "\x00Primero", 8);
    frame(&t, 3, "TPE1", 0, "\x00" "Segundo", 8);
    end_tag(&t, 500);
    memset(&tg, 0, sizeof tg);
    total = id3_parse_v2(t.b, 10 + 18 + 4, &tg);               /* corta dentro del segundo frame */
    CHECK(total == (long)t.n && !strcmp(tg.title, "Primero") && tg.artist[0] == '\0',
          "truncada: total %ld titulo '%s' artista '%s'", total, tg.title, tg.artist);

    /* no es ID3 */
    memset(&tg, 0, sizeof tg);
    CHECK(id3_parse_v2((const uint8_t *)"\xFF\xFB\x90\x00xxxxxxxxxxxxxxxx", 20, &tg) == 0, "no ID3 -> 0");
    CHECK(id3_v2_total_size((const uint8_t *)"ID3\x03\x00\x00\x00\x00\x02\x00", 10) == 266, "tamano total 10+256");

    /* ID3v1 (solo completa lo vacio) */
    {
        uint8_t v1[128];

        memset(v1, 0, sizeof v1);
        memcpy(v1, "TAG", 3);
        memcpy(v1 + 3, "Song", 4);
        memcpy(v1 + 33, "\xC9" "douard", 7);
        memset(&tg, 0, sizeof tg);
        strcpy(tg.title, "Ya esta");
        CHECK(id3_parse_v1(v1, &tg) == 1, "ID3v1 presente");
        CHECK(!strcmp(tg.title, "Ya esta") && !strcmp(tg.artist, "\x83" "douard"),
              "ID3v1 no pisa lo existente: '%s' / '%s'", tg.title, tg.artist);
        v1[0] = 'X';
        CHECK(id3_parse_v1(v1, &tg) == 0, "ID3v1 ausente");
    }

    /* conversiones puntuales */
    CHECK(id3_cp_to_macroman(0x00F1) == 0x96, "n con tilde");
    CHECK(id3_cp_to_macroman(0x2019) == 0xD5, "apostrofo tipografico");
    CHECK(id3_cp_to_macroman(0x4E2D) == '?', "no representable -> ?");
    CHECK(id3_latin1_to_macroman(0x92) == 0xD5, "CP1252 0x92 -> apostrofo");
}

/* ---- order ---- */
static void test_order(void)
{
    Order o;
    unsigned i, seen[ORDER_MAX];
    int v, k;

    order_init(&o, 7);
    for (i = 0; i < 10; i++)
        CHECK(order_add(&o) == (int)i, "add %u", i);
    CHECK(order_current(&o) == -1, "sin actual al inicio");

    /* orden normal, sin repetir */
    CHECK(order_next(&o, 1) == 0, "primera = 0");
    for (i = 1; i < 10; i++)
        CHECK(order_next(&o, 1) == (int)i, "siguiente %u", i);
    CHECK(order_next(&o, 1) == -1, "al final sin repeat -> -1");
    CHECK(order_current(&o) == 9, "no avanza al fallar");

    /* repeat all da la vuelta; repeat one repite solo en automatico */
    order_set_repeat(&o, REPEAT_ALL);
    CHECK(order_next(&o, 1) == 0, "repeat all vuelve a 0");
    CHECK(order_prev(&o) == 9, "prev desde 0 con repeat all -> ultima");
    order_set_repeat(&o, REPEAT_ONE);
    order_set_current(&o, 4);
    CHECK(order_next(&o, 1) == 4, "repeat one automatico repite");
    CHECK(order_next(&o, 0) == 5, "repeat one manual avanza");
    order_set_repeat(&o, REPEAT_OFF);
    order_set_current(&o, 0);
    CHECK(order_prev(&o) == 0, "prev en la primera sin repeat se queda");

    /* shuffle: una vuelta completa visita todas exactamente una vez, la actual primero */
    order_set_current(&o, 3);
    order_set_shuffle(&o, 1);
    CHECK(order_current(&o) == 3, "shuffle conserva la actual");
    memset(seen, 0, sizeof seen);
    seen[3] = 1;
    for (k = 0; k < 9; k++) {
        v = order_next(&o, 1);
        CHECK(v >= 0 && v < 10 && !seen[v], "shuffle repite o se sale: %d", v);
        if (v >= 0) seen[v] = 1;
    }
    CHECK(order_next(&o, 1) == -1, "shuffle sin repeat termina tras una vuelta");
    {
        int cur = order_current(&o);

        order_set_shuffle(&o, 0);
        CHECK(order_current(&o) == cur, "apagar shuffle conserva la actual");
        v = order_next(&o, 0);
        CHECK(v == (cur < 9 ? cur + 1 : -1), "tras apagar shuffle sigue el orden normal: %d", v);
    }

    /* shuffle con repeat all: no repite la misma al dar la vuelta */
    order_init(&o, 99);
    for (i = 0; i < 6; i++) order_add(&o);
    order_set_repeat(&o, REPEAT_ALL);
    order_set_shuffle(&o, 1);
    {
        int last = order_next(&o, 1);
        int bad = 0;
        for (k = 0; k < 200; k++) {
            v = order_next(&o, 1);
            if (v == last) bad++;
            last = v;
        }
        CHECK(bad == 0, "repeat all + shuffle repitio la misma pista %d veces seguidas", bad);
    }

    /* borrado: mantiene la actual y corre los indices */
    order_init(&o, 1);
    for (i = 0; i < 5; i++) order_add(&o);
    order_set_current(&o, 3);
    order_remove(&o, 1);
    CHECK(order_current(&o) == 2, "borrar antes de la actual la corre: %d", order_current(&o));
    order_remove(&o, 2);                                    /* borra la actual (era 3, ahora 2) */
    CHECK(order_current(&o) == -1, "al borrar la actual queda sin actual");
    v = order_next(&o, 0);
    CHECK(v == 2, "siguiente tras borrar la actual: %d (esperaba 2, la que era 4)", v);
    order_remove(&o, 0);
    order_remove(&o, 0);
    order_remove(&o, 0);
    CHECK(o.count == 0 && order_next(&o, 1) == -1, "lista vacia");

    /* limite de capacidad */
    order_init(&o, 5);
    for (i = 0; i < ORDER_MAX; i++)
        if (order_add(&o) != (int)i) { CHECK(0, "add %u fallo", i); break; }
    CHECK(order_add(&o) == -1, "lista llena -> -1");
}

int main(void)
{
    test_id3();
    test_order();
    printf(failures ? "RESULTADO: %d FALLAS\n" : "RESULTADO: todo OK (%d fallas)\n", failures);
    return failures ? 1 : 0;
}
