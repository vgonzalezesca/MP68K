# MP368K — build con Retro68 (m68k-apple-macos).
# Uso normal: tools/build.sh

cmake_minimum_required(VERSION 3.13)
# CXX hace falta aunque no haya fuentes C++: add_application(... CONSOLE) enlaza
# RetroConsole con LINKER_LANGUAGE CXX.
project(MP368K C CXX)

if(NOT CMAKE_SYSTEM_NAME MATCHES Retro68)
    message(FATAL_ERROR
        "Configurar con el toolchain de Retro68 (68k), por ejemplo:\n"
        "  cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="
        "$HOME/Retro68-build/toolchain/m68k-apple-macos/cmake/retro68.toolchain.cmake\n"
        "o simplemente: tools/build.sh")
endif()

# --- Flags del 68LC040 sin FPU (ver docs/DECISIONS.md, ADR-005) -------------
set(MP368K_CPU_FLAGS  -m68040 -msoft-float)
set(MP368K_OPT_FLAGS  -O2 -fomit-frame-pointer -ffunction-sections -fdata-sections)
set(MP368K_WARN_FLAGS -Wall -Wextra -Wno-unused-parameter -Wno-multichar)

add_compile_options(${MP368K_CPU_FLAGS} ${MP368K_OPT_FLAGS} ${MP368K_WARN_FLAGS})
# Se conservan los nombres para MacsBug (sin --mac-strip-macsbug) para depurar.
add_link_options(${MP368K_CPU_FLAGS} -Wl,-gc-sections)

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
string(REPLACE ";" " " MP368K_FLAGS_STR "${MP368K_CPU_FLAGS};${MP368K_OPT_FLAGS}")

configure_file(src/common/build_info.h.in ${CMAKE_BINARY_DIR}/generated/build_info.h @ONLY)

# --- Código común -------------------------------------------------------------
add_library(mp368k_common STATIC
    src/common/textlog.c
    src/common/mactime.c
    src/common/appfile.c
    src/common/macenv.c
    src/common/refcrc.c
    src/common/memblock.c
    src/common/fileread.c
    src/common/audio.c
    src/ring.c
    src/pcmconv.c)
target_include_directories(mp368k_common PUBLIC
    src
    src/common
    ${CMAKE_BINARY_DIR}/generated)

# --- Etapa 0: sysprobe ------------------------------------------------------
add_application(sysprobe
    src/sysprobe/sysprobe.c
    CONSOLE
    CREATOR "M368")
target_link_libraries(sysprobe mp368k_common)

# --- Etapa 1: microbenchmarks de CPU ----------------------------------------
add_application(microbench
    bench/microbench.c
    CONSOLE
    CREATOR "M368")
target_link_libraries(microbench mp368k_common)

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
add_application(decbench
    bench/decbench.c
    CONSOLE
    CREATOR "M368")
target_link_libraries(decbench decoder_core mp368k_common)

# --- Etapa 3: perfilado por etapas --------------------------------------------
# Copia parcheada de libmad con marcas de tiempo (el original no se modifica).
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

# --- Etapa 5a: prueba del camino de audio -------------------------------------
add_application(audiotest
    src/player/audiotest.c
    CONSOLE
    CREATOR "M368")
target_link_libraries(audiotest decoder_core mp368k_common)

# --- Etapa 5b: motor del reproductor (la interfaz se agrega en el siguiente paso) -----
add_library(player_engine STATIC src/player/player.c src/id3.c src/mp3info.c src/order.c)
target_include_directories(player_engine PUBLIC src/player)
target_link_libraries(player_engine PUBLIC decoder_core mp368k_common)

# --- Etapa 5b-1: el reproductor con ventana ---------------------------------------
add_application(mp368k
    src/player/app.c
    src/player/ui_main.c
    src/player/ui_list.c
    src/player/ui_draw.c
    src/player/playlist.c
    src/player/trackinfo.c
    src/player/player.r
    CREATOR "M368")
target_link_libraries(mp368k player_engine)
