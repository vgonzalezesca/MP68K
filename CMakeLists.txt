# MP368K — build con Retro68. Esta parte es común: lo propio de cada equipo está en
# targets/target.cmake, que solo existe en las ramas 68k y ppc (ADR-007).
# Uso normal: tools/build.sh desde una de esas ramas, o tools/build-all.sh desde cualquiera.

cmake_minimum_required(VERSION 3.13)
# CXX hace falta aunque no haya fuentes C++: add_application(... CONSOLE) enlaza
# RetroConsole con LINKER_LANGUAGE CXX.
project(MP368K C CXX)

if(NOT EXISTS ${CMAKE_SOURCE_DIR}/targets/target.cmake)
    message(FATAL_ERROR
        "Esta es la rama general: no compila aplicaciones Mac (solo tools/host-*.sh).\n"
        "Compile desde la rama 68k o ppc con tools/build.sh, o use tools/build-all.sh.")
endif()

# Define MP368K_TARGET, MP368K_SYSTEM_NAME, MP368K_CPU_FLAGS, MP368K_OPT_FLAGS,
# MP368K_LINK_FLAGS, MP368K_APPS y la función mp368k_target_apps().
include(targets/target.cmake)

# Selector C/asm (ADR-008, ADR-014): -DMP368K_ASM=OFF vuelve a la libmad original y a la referencia en C.
option(MP368K_ASM "Usar los núcleos en ensamblador del equipo, si los hay" ON)
if(MP368K_ASM_KERNEL_SOURCES OR MP368K_ASM_COMMON_SOURCES)
    enable_language(ASM)
endif()

if(NOT CMAKE_SYSTEM_NAME STREQUAL MP368K_SYSTEM_NAME)
    message(FATAL_ERROR
        "La rama ${MP368K_TARGET} se compila con el toolchain ${MP368K_SYSTEM_NAME}, no con "
        "'${CMAKE_SYSTEM_NAME}'. Use tools/build.sh.")
endif()

set(MP368K_WARN_FLAGS -Wall -Wextra -Wno-unused-parameter -Wno-multichar)

add_compile_options(${MP368K_CPU_FLAGS} ${MP368K_OPT_FLAGS} ${MP368K_WARN_FLAGS})
add_link_options(${MP368K_CPU_FLAGS} ${MP368K_LINK_FLAGS})

# --- Identificación del build (va en cada archivo de resultados) ------------
execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE MP368K_GIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
if(NOT MP368K_GIT_HASH)
    set(MP368K_GIT_HASH "sin-git")
else()
    execute_process(
        COMMAND git status --porcelain --untracked-files=no
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE MP368K_GIT_DIRTY
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    if(MP368K_GIT_DIRTY)
        string(APPEND MP368K_GIT_HASH "-mod")
    endif()
endif()
set(MP368K_GIT_HASH "${MP368K_TARGET}-${MP368K_GIT_HASH}")
string(REPLACE ";" " " MP368K_FLAGS_STR "${MP368K_CPU_FLAGS};${MP368K_OPT_FLAGS}")

configure_file(src/common/build_info.h.in ${CMAKE_BINARY_DIR}/generated/build_info.h @ONLY)

# --- Código común -------------------------------------------------------------
add_library(mp368k_common STATIC
    targets/mactime.c
    targets/sndver.c
    src/common/textlog.c
    src/common/mactime.c
    src/common/appfile.c
    src/common/macenv.c
    src/common/refcrc.c
    src/common/memblock.c
    src/common/fileread.c
    src/common/audio.c
    src/ring.c
    src/pcmconv.c
    src/pcm_ref.c)
target_include_directories(mp368k_common PUBLIC
    src
    src/common
    ${CMAKE_BINARY_DIR}/generated)

# Conversiones de PCM en asm (u8 y ganancia de la salida): solo las usa pcmconv.c, la salida de audio.
if(MP368K_ASM AND MP368K_ASM_COMMON_SOURCES)
    target_sources(mp368k_common PRIVATE ${MP368K_ASM_COMMON_SOURCES})
    set_source_files_properties(src/pcmconv.c PROPERTIES COMPILE_DEFINITIONS "${MP368K_ASM_COMMON_DEFS}")
endif()

add_library(mp368k_hooks STATIC targets/hooks.c)
target_include_directories(mp368k_hooks PRIVATE src)

# Reproducción por QuickTime (ADR-010): solo donde el equipo la implemente (68k: targets/qtplay.c). Sin ese archivo,
# src/qtplay.h deja reemplazos que dicen "QuickTime no está instalado" y el reproductor funciona como antes.
if(EXISTS ${CMAKE_SOURCE_DIR}/targets/qtplay.c)
    add_library(mp368k_qtplay STATIC targets/qtplay.c)
    target_include_directories(mp368k_qtplay PRIVATE src)
    add_compile_definitions(MP368K_HAVE_QT=1)
endif()

# --- Etapa 0: sysprobe ------------------------------------------------------
if(sysprobe IN_LIST MP368K_APPS)
    add_application(sysprobe
        src/sysprobe/sysprobe.c
        CONSOLE
        CREATOR "M368")
    target_link_libraries(sysprobe mp368k_common)
endif()

# --- libmad (third_party/libmad, 0.15.1b, GPL; ver ADR-006) -------------------
# Sin configure: se compilan los .c directamente, sin decoder.c (usa fork/pipe).
# mad.h se regenera porque el del tarball trae FPM_INTEL fijo.
set(MAD_FPM FPM_64BIT)
set(MAD_DIR ${CMAKE_SOURCE_DIR}/third_party/libmad)
set(MAD_GEN_DIR ${CMAKE_BINARY_DIR}/libmad)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/tools/gen-mad-h.sh ${MAD_DIR}/mad.h.sed)
execute_process(
    COMMAND ${CMAKE_SOURCE_DIR}/tools/gen-mad-h.sh ${MAD_GEN_DIR}/mad.h ${MAD_FPM}
    RESULT_VARIABLE MAD_GEN_RESULT)
if(NOT MAD_GEN_RESULT EQUAL 0)
    message(FATAL_ERROR "tools/gen-mad-h.sh fallo")
endif()

add_library(mad STATIC
    ${MAD_DIR}/bit.c ${MAD_DIR}/fixed.c ${MAD_DIR}/frame.c ${MAD_DIR}/huffman.c
    ${MAD_DIR}/layer12.c ${MAD_DIR}/layer3.c ${MAD_DIR}/stream.c ${MAD_DIR}/synth.c
    ${MAD_DIR}/timer.c ${MAD_DIR}/version.c)
target_include_directories(mad PRIVATE ${MAD_DIR})
target_include_directories(mad PUBLIC ${MAD_GEN_DIR})
# NDEBUG apaga assert(); los SIZEOF_* coinciden con los de mad.h generado.
target_compile_definitions(mad PRIVATE ${MAD_FPM} NDEBUG HAVE_LIMITS_H
    SIZEOF_INT=4 SIZEOF_LONG=4 SIZEOF_LONG_LONG=8)
target_compile_options(mad PRIVATE -w)      # código de terceros: sin avisos

add_library(decoder_core STATIC src/decoder_core.c)
target_include_directories(decoder_core PUBLIC src)
target_link_libraries(decoder_core PUBLIC mad)

# --- Etapa 2: banco del decodificador (sin audio) -----------------------------
if(decbench IN_LIST MP368K_APPS)
    add_application(decbench
        bench/decbench.c
        CONSOLE
        CREATOR "M368")
    target_link_libraries(decbench decoder_core mp368k_common)
endif()

# --- Etapa 3: perfilado por etapas --------------------------------------------
# Copia parcheada de libmad con marcas de tiempo (el original no se modifica).
if(decprofile IN_LIST MP368K_APPS)
    set(MAD_PROF_DIR ${CMAKE_BINARY_DIR}/libmad_prof)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        ${CMAKE_SOURCE_DIR}/tools/prepare-libmad-prof.sh
        ${CMAKE_SOURCE_DIR}/third_party/patches/libmad-0.15.1b-profile.patch
        ${CMAKE_SOURCE_DIR}/third_party/patches/mp368k_prof.h)
    execute_process(
        COMMAND ${CMAKE_SOURCE_DIR}/tools/prepare-libmad-prof.sh ${MAD_PROF_DIR}
        RESULT_VARIABLE MAD_PROF_RESULT)
    if(NOT MAD_PROF_RESULT EQUAL 0)
        message(FATAL_ERROR "tools/prepare-libmad-prof.sh fallo")
    endif()

    add_library(mad_prof STATIC
        ${MAD_PROF_DIR}/bit.c ${MAD_PROF_DIR}/fixed.c ${MAD_PROF_DIR}/frame.c
        ${MAD_PROF_DIR}/huffman.c ${MAD_PROF_DIR}/layer12.c ${MAD_PROF_DIR}/layer3.c
        ${MAD_PROF_DIR}/stream.c ${MAD_PROF_DIR}/synth.c ${MAD_PROF_DIR}/timer.c
        ${MAD_PROF_DIR}/version.c ${MAD_PROF_DIR}/mp368k_prof.c)
    target_include_directories(mad_prof PRIVATE ${MAD_PROF_DIR})
    target_include_directories(mad_prof PUBLIC ${MAD_GEN_DIR})
    target_compile_definitions(mad_prof PRIVATE ${MAD_FPM} NDEBUG HAVE_LIMITS_H
        SIZEOF_INT=4 SIZEOF_LONG=4 SIZEOF_LONG_LONG=8 MP368K_PROFILE)
    target_compile_options(mad_prof PRIVATE -w)

    add_library(decoder_core_prof STATIC src/decoder_core.c)
    target_include_directories(decoder_core_prof PUBLIC src)
    target_compile_definitions(decoder_core_prof PRIVATE MP368K_PROFILE)
    target_link_libraries(decoder_core_prof PUBLIC mad_prof)

    add_application(decprofile
        bench/decprofile.c
        CONSOLE
        CREATOR "M368")
    target_link_libraries(decprofile decoder_core_prof mp368k_common)
endif()


# --- Etapa 6: capa de códecs (MP3, FLAC) ---------------------------------------
# dr_flac (dominio público/MIT-0, third_party/dr_libs) se usa con una copia sin `double` que genera
# tools/prepare-dr-flac.sh; el original no se modifica. Se compila sin avisos (código de terceros).
set(DRFLAC_DIR ${CMAKE_BINARY_DIR}/drflac)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/tools/prepare-dr-flac.sh ${CMAKE_SOURCE_DIR}/third_party/dr_libs/dr_flac.h)
execute_process(
    COMMAND ${CMAKE_SOURCE_DIR}/tools/prepare-dr-flac.sh ${DRFLAC_DIR}
    RESULT_VARIABLE DRFLAC_GEN_RESULT OUTPUT_QUIET)
if(NOT DRFLAC_GEN_RESULT EQUAL 0)
    message(FATAL_ERROR "tools/prepare-dr-flac.sh fallo")
endif()

# libmad con el gancho del ecualizador (ADR-011): otra copia parcheada, sin tocar third_party/libmad/.
# Con el gancho en NULL el PCM es idéntico al de la libmad original. decbench, decprofile y audiotest
# siguen con la original.
set(MAD_EQ_DIR ${CMAKE_BINARY_DIR}/libmad_eq)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/tools/prepare-libmad-eq.sh
    ${CMAKE_SOURCE_DIR}/third_party/patches/libmad-0.15.1b-eq.patch
    ${CMAKE_SOURCE_DIR}/third_party/patches/libmad-0.15.1b-synth.patch
    ${CMAKE_SOURCE_DIR}/third_party/patches/libmad-0.15.1b-imdct.patch
    ${CMAKE_SOURCE_DIR}/third_party/patches/libmad-0.15.1b-l3.patch
    ${CMAKE_SOURCE_DIR}/third_party/patches/mp368k_eq.h
    ${CMAKE_SOURCE_DIR}/third_party/patches/mp368k_l3.h)
execute_process(
    COMMAND ${CMAKE_SOURCE_DIR}/tools/prepare-libmad-eq.sh ${MAD_EQ_DIR}
    RESULT_VARIABLE MAD_EQ_RESULT)
if(NOT MAD_EQ_RESULT EQUAL 0)
    message(FATAL_ERROR "tools/prepare-libmad-eq.sh fallo")
endif()

# Tremor (rama lowmem) con los cambios de MP368K (ADR-009): otra copia parcheada, sin tocar third_party/tremor/.
set(TREMOR_DIR ${CMAKE_BINARY_DIR}/tremor)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/tools/prepare-tremor.sh
    ${CMAKE_SOURCE_DIR}/third_party/patches/tremor-lowmem-mp368k.patch
    ${CMAKE_SOURCE_DIR}/third_party/patches/mp368k_tremor.h)
execute_process(
    COMMAND ${CMAKE_SOURCE_DIR}/tools/prepare-tremor.sh ${TREMOR_DIR}
    RESULT_VARIABLE TREMOR_RESULT)
if(NOT TREMOR_RESULT EQUAL 0)
    message(FATAL_ERROR "tools/prepare-tremor.sh fallo")
endif()
add_library(tremor STATIC
    ${TREMOR_DIR}/bitwise.c ${TREMOR_DIR}/codebook.c ${TREMOR_DIR}/dsp.c ${TREMOR_DIR}/floor0.c
    ${TREMOR_DIR}/floor1.c ${TREMOR_DIR}/floor_lookup.c ${TREMOR_DIR}/framing.c ${TREMOR_DIR}/info.c
    ${TREMOR_DIR}/mapping0.c ${TREMOR_DIR}/mdct.c ${TREMOR_DIR}/misc.c ${TREMOR_DIR}/res012.c
    ${TREMOR_DIR}/vorbisfile.c)
target_include_directories(tremor PRIVATE ${TREMOR_DIR})
target_compile_definitions(tremor PRIVATE MP368K_TREMOR)
target_compile_options(tremor PRIVATE -w -fwrapv)      # código de terceros: sin avisos; la envoltura de enteros es la esperada

# libwma (Rockbox) con el float reemplazado por enteros y cabeceras de reemplazo (ADR-009): otra copia parcheada.
set(WMA_DIR ${CMAKE_BINARY_DIR}/libwma)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/tools/prepare-libwma.sh
    ${CMAKE_SOURCE_DIR}/third_party/patches/libwma-mp368k.patch
    ${CMAKE_SOURCE_DIR}/third_party/patches/libwma_shim/codecs.h
    ${CMAKE_SOURCE_DIR}/third_party/patches/libwma_shim/platform.h
    ${CMAKE_SOURCE_DIR}/third_party/patches/libwma_shim/codecs/lib/codeclib.h)
execute_process(
    COMMAND ${CMAKE_SOURCE_DIR}/tools/prepare-libwma.sh ${WMA_DIR}
    RESULT_VARIABLE WMA_RESULT)
if(NOT WMA_RESULT EQUAL 0)
    message(FATAL_ERROR "tools/prepare-libwma.sh fallo")
endif()
set(WMA_SOURCES
    ${WMA_DIR}/wmadeci.c ${WMA_DIR}/wmafixed.c ${WMA_DIR}/ffmpeg_bitstream.c ${WMA_DIR}/mdct.c
    ${WMA_DIR}/mdct_lookup.c ${WMA_DIR}/fft-ffmpeg.c)

# Núcleos de la síntesis del MP3 (ADR-014): por omisión, la libmad original en C. Si el equipo trae asm
# (MP368K_ASM_KERNEL_SOURCES y MP368K_ASM_KERNEL_DEFS, definidos en targets/target.cmake), la pila principal los
# usa y se arma además una pila "_ref" con la libmad original para comparar las dos en el mismo equipo
# (codecbench_ref). Selector de compilación: -DMP368K_ASM=OFF vuelve a C.
function(mp368k_codec_stack SFX KERNEL_LABEL)
    cmake_parse_arguments(K "" "" "SOURCES;DEFS;COREDEFS;CODECDEFS;WMASOURCES;WMADEFS" ${ARGN})
    # Las ventanas de 16x16 bits de los perfiles reducidos no existen en la libmad original: sin asm se usan sus referencias en C.
    if(NOT K_DEFS MATCHES "MP368K_SYNTH_WINDOW_HALF16=")
        list(APPEND K_SOURCES src/synth_ref.c)
        list(APPEND K_DEFS MP368K_SYNTH_WINDOW_HALF16=mp368k_synth_window_half16_ref
            MP368K_SYNTH_WINDOW_FULL16=mp368k_synth_window_full16_ref)
    endif()
    add_library(mad_eq${SFX} STATIC
        ${MAD_EQ_DIR}/bit.c ${MAD_EQ_DIR}/fixed.c ${MAD_EQ_DIR}/frame.c ${MAD_EQ_DIR}/huffman.c
        ${MAD_EQ_DIR}/layer12.c ${MAD_EQ_DIR}/layer3.c ${MAD_EQ_DIR}/stream.c ${MAD_EQ_DIR}/synth.c
        ${MAD_EQ_DIR}/timer.c ${MAD_EQ_DIR}/version.c ${K_SOURCES})
    target_include_directories(mad_eq${SFX} PRIVATE ${MAD_EQ_DIR} ${CMAKE_SOURCE_DIR}/src)
    target_include_directories(mad_eq${SFX} PUBLIC ${MAD_GEN_DIR})
    target_compile_definitions(mad_eq${SFX} PRIVATE ${MAD_FPM} NDEBUG HAVE_LIMITS_H
        SIZEOF_INT=4 SIZEOF_LONG=4 SIZEOF_LONG_LONG=8 ${K_DEFS})
    target_compile_options(mad_eq${SFX} PRIVATE -w)

    # libwma de esta pila: con núcleos asm (ventana de la IMDCT...) o con su código original en C
    add_library(wma${SFX} STATIC ${WMA_SOURCES} ${K_WMASOURCES})
    target_include_directories(wma${SFX} PRIVATE ${WMA_DIR} ${WMA_DIR}/codecs/lib ${CMAKE_SOURCE_DIR}/src)
    target_compile_definitions(wma${SFX} PRIVATE ROCKBOX ${K_WMADEFS})
    target_compile_options(wma${SFX} PRIVATE -w -fwrapv)    # código de terceros: sin avisos; la envoltura de enteros es la esperada

    add_library(decoder_core_eq${SFX} STATIC src/decoder_core.c)
    target_include_directories(decoder_core_eq${SFX} PUBLIC src)
    target_include_directories(decoder_core_eq${SFX} PRIVATE ${MAD_EQ_DIR})
    target_compile_definitions(decoder_core_eq${SFX} PRIVATE MP368K_L3)
    if(K_COREDEFS)
        target_compile_definitions(decoder_core_eq${SFX} PRIVATE ${K_COREDEFS})
    endif()
    # mp368k_common trae pcm_ref.c / pcm_040.s: debe quedar DESPUÉS de decoder_core_eq en la línea de enlace
    target_link_libraries(decoder_core_eq${SFX} PUBLIC mad_eq${SFX} mp368k_common)

    add_library(codecs${SFX} STATIC
        src/arena.c src/heap.c src/codec.c src/codec_mp3.c src/codec_flac.c src/codec_vorbis.c src/codec_wma.c src/asfparse.c
        src/vorbiscomment.c
        src/id3.c src/mp3info.c src/eq.c src/eq_ref.c src/dr_flac_impl.c)
    target_include_directories(codecs${SFX} PUBLIC src ${DRFLAC_DIR})
    target_compile_definitions(codecs${SFX} PUBLIC MP368K_SYNTH_KERNEL_LABEL="${KERNEL_LABEL}")
    if(K_CODECDEFS)
        target_compile_definitions(codecs${SFX} PRIVATE ${K_CODECDEFS})
    endif()
    target_link_libraries(codecs${SFX} PUBLIC decoder_core_eq${SFX} tremor wma${SFX})
endfunction()

if(MP368K_ASM AND MP368K_ASM_KERNEL_SOURCES)
    mp368k_codec_stack("" asm SOURCES ${MP368K_ASM_KERNEL_SOURCES} DEFS ${MP368K_ASM_KERNEL_DEFS}
        COREDEFS ${MP368K_ASM_CORE_DEFS} CODECDEFS ${MP368K_ASM_CODEC_DEFS}
        WMASOURCES ${MP368K_ASM_WMA_SOURCES} WMADEFS ${MP368K_ASM_WMA_DEFS})
    mp368k_codec_stack(_ref c)
    set(MP368K_HAVE_REF_STACK TRUE)
else()
    mp368k_codec_stack("" c)
endif()
set_source_files_properties(src/dr_flac_impl.c PROPERTIES COMPILE_OPTIONS -w)
set_source_files_properties(src/codec_vorbis.c PROPERTIES COMPILE_OPTIONS "-I${TREMOR_DIR};-DMP368K_TREMOR")
set_source_files_properties(src/codec_wma.c PROPERTIES COMPILE_OPTIONS "-I${WMA_DIR};-I${WMA_DIR}/codecs/lib;-DROCKBOX")

# --- Etapa 5a: prueba del camino de audio, con la pila del reproductor -----------
# audiotest decodifica como el reproductor (núcleos asm si los hay); audiotest_ref, con la pila en C, para comparar.
if(audiotest IN_LIST MP368K_APPS)
    add_application(audiotest
        src/player/audiotest.c
        CONSOLE
        CREATOR "M368")
    if(MP368K_HAVE_REF_STACK)
        target_compile_definitions(audiotest PRIVATE MP368K_SYNTH_KERNEL_LABEL="asm")
    else()
        target_compile_definitions(audiotest PRIVATE MP368K_SYNTH_KERNEL_LABEL="c")
    endif()
    target_link_libraries(audiotest decoder_core_eq mp368k_common)
endif()
if(MP368K_HAVE_REF_STACK AND audiotest_ref IN_LIST MP368K_APPS)
    add_application(audiotest_ref
        src/player/audiotest.c
        CONSOLE
        CREATOR "M368")
    target_compile_definitions(audiotest_ref PRIVATE AUDIOTEST_OUT_NAME="audiotest_results_ref.txt" MP368K_SYNTH_KERNEL_LABEL="c")
    target_link_libraries(audiotest_ref decoder_core_eq_ref mp368k_common)
endif()

# --- Etapa 6: banco de los códecs (velocidad y CRC contra el PC, sin audio) ------
if(codecbench IN_LIST MP368K_APPS)
    add_application(codecbench
        bench/codecbench.c
        CONSOLE
        CREATOR "M368")
    target_link_libraries(codecbench codecs mp368k_common)
endif()
if(MP368K_HAVE_REF_STACK AND codecbench_ref IN_LIST MP368K_APPS)
    add_application(codecbench_ref
        bench/codecbench.c
        CONSOLE
        CREATOR "M368")
    target_compile_definitions(codecbench_ref PRIVATE CODECBENCH_OUT_NAME="bench_results_codecs_ref.txt")
    target_link_libraries(codecbench_ref codecs_ref mp368k_common)
endif()

# --- Etapa 5b: el reproductor ---------------------------------------------------
if(mp368k IN_LIST MP368K_APPS)
    add_library(player_engine STATIC src/player/player.c src/order.c src/qtfmt.c)
    target_include_directories(player_engine PUBLIC src/player)
    target_link_libraries(player_engine PUBLIC codecs mp368k_common)
    if(TARGET mp368k_qtplay)
        target_link_libraries(player_engine PUBLIC mp368k_qtplay)
    endif()

    add_application(mp368k
        src/player/app.c
        src/player/ui_main.c
        src/player/ui_list.c
        src/player/ui_about.c
        src/player/ui_eq.c
        src/player/ui_draw.c
        src/player/playlist.c
        src/player/trackinfo.c
        src/player/player.r
        src/player/icons.r
        CREATOR "M368")
    target_link_libraries(mp368k player_engine mp368k_hooks)
endif()

# --- Lo que solo existe en este equipo ------------------------------------------
mp368k_target_apps()
