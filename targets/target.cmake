# Rama 68k: Macintosh LC 580, 68LC040 sin FPU (ver TARGET.md y docs/DECISIONS.md, ADR-005).
set(MP368K_TARGET 68k)
set(MP368K_SYSTEM_NAME Retro68)
set(MP368K_CPU_FLAGS -m68040 -msoft-float)
set(MP368K_OPT_FLAGS -O2 -fomit-frame-pointer -ffunction-sections -fdata-sections)
# Se conservan los nombres para MacsBug (sin --mac-strip-macsbug) para depurar.
set(MP368K_LINK_FLAGS -Wl,-gc-sections)
set(MP368K_APPS sysprobe microbench decbench decprofile audiotest audiotest_ref codecbench codecbench_ref mp368k segtest qttest)

# Núcleos en ensamblador 68040 (ADR-008, ADR-014). Con -DMP368K_ASM=OFF el build usa la libmad original en C.
# Con asm activo también se arman codecbench_ref y audiotest_ref (misma pila en C) para compararlos en el mismo equipo.
set(MP368K_ASM_KERNEL_SOURCES targets/asm/synth_window_040.s targets/asm/synth_window_half_040.s targets/asm/dct32_040.s
    targets/asm/dct32_half_040.s targets/asm/synth_window_half16_040.s targets/asm/synth_window_full16_040.s
    targets/asm/imdct_l_040.s)
set(MP368K_ASM_KERNEL_DEFS MP368K_SYNTH_WINDOW=mp368k_synth_window_full_040 MP368K_SYNTH_WINDOW_HALF=mp368k_synth_window_half_040 MP368K_DCT32=mp368k_dct32_040
    MP368K_DCT32_HALF=mp368k_dct32_half_040 MP368K_SYNTH_WINDOW_HALF16=mp368k_synth_window_half16_040
    MP368K_SYNTH_WINDOW_FULL16=mp368k_synth_window_full16_040 MP368K_IMDCT_L=mp368k_imdct_l_040)

# Conversiones de PCM: la de la salida de la síntesis se elige por pila (solo la pila asm); las de 8 bits y ganancia,
# que usa la salida de audio, van en mp368k_common.
set(MP368K_ASM_COMMON_SOURCES targets/asm/pcm_040.s targets/asm/eq_040.s)
set(MP368K_ASM_COMMON_DEFS MP368K_PCM_U8=mp368k_pcm_u8_040 MP368K_PCM_GAIN16=mp368k_pcm_gain16_040)
set(MP368K_ASM_CORE_DEFS MP368K_PCM_SCALE16=mp368k_pcm_scale16_040)
# Núcleos de libwma (ventana de la IMDCT): se compilan dentro de la biblioteca wma de la pila asm; la pila _ref usa el código original.
set(MP368K_ASM_WMA_SOURCES targets/asm/wma_window_040.s targets/asm/wma_fft_040.s)
set(MP368K_ASM_WMA_DEFS MP368K_WMA_FMUL_ADD_ADD=mp368k_wma_fmul_add_add_040 MP368K_WMA_FMUL_REVERSE=mp368k_wma_fmul_reverse_040
    MP368K_WMA_FFT_PASS=mp368k_wma_fft_pass_040)
# Lo que se compila en la biblioteca de códecs (codec_wma.c y eq.c); solo la pila asm.
set(MP368K_ASM_CODEC_DEFS MP368K_PCM_WMA16=mp368k_pcm_wma16_040 MP368K_EQ_MULG_RANGE=mp368k_eq_mulg_range_040)

function(mp368k_target_apps)
    # Regla 6: comprobar que un segmento de código de más de 100 KB corre (targets/segtest.c).
    set(SEGTEST_DIR ${CMAKE_BINARY_DIR}/segtest)
    file(MAKE_DIRECTORY ${SEGTEST_DIR})
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        ${CMAKE_SOURCE_DIR}/targets/gen_segtest.py)
    execute_process(
        COMMAND python3 -I ${CMAKE_SOURCE_DIR}/targets/gen_segtest.py ${SEGTEST_DIR}/segtest_gen.c 700
        RESULT_VARIABLE SEGTEST_GEN_RESULT)
    if(NOT SEGTEST_GEN_RESULT EQUAL 0)
        message(FATAL_ERROR "targets/gen_segtest.py fallo")
    endif()
    add_application(segtest
        targets/segtest.c
        ${SEGTEST_DIR}/segtest_gen.c
        CONSOLE
        CREATOR "M368")
    target_include_directories(segtest PRIVATE ${SEGTEST_DIR})
    target_link_libraries(segtest mp368k_common)

    # Fase 6: diagnóstico de QuickTime (solo funciones ya declaradas por las Multiversal; ver targets/qttest.c).
    add_application(qttest
        targets/qttest.c
        CONSOLE
        CREATOR "M368")
    target_link_libraries(qttest mp368k_common)

    # Etapa 1: microbenchmarks de CPU. Mide instrucciones 68k: no aplica en PowerPC.
    add_application(microbench
        targets/microbench.c
        CONSOLE
        CREATOR "M368")
    target_link_libraries(microbench mp368k_common)
endfunction()
