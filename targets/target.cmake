# Rama 68k: Macintosh LC 580, 68LC040 sin FPU (ver TARGET.md y docs/DECISIONS.md, ADR-005).
set(MP368K_TARGET 68k)
set(MP368K_SYSTEM_NAME Retro68)
set(MP368K_CPU_FLAGS -m68040 -msoft-float)
set(MP368K_OPT_FLAGS -O2 -fomit-frame-pointer -ffunction-sections -fdata-sections)
# Se conservan los nombres para MacsBug (sin --mac-strip-macsbug) para depurar.
set(MP368K_LINK_FLAGS -Wl,-gc-sections)
set(MP368K_APPS sysprobe microbench decbench decprofile audiotest mp368k segtest)

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

    # Etapa 1: microbenchmarks de CPU. Mide instrucciones 68k: no aplica en PowerPC.
    add_application(microbench
        targets/microbench.c
        CONSOLE
        CREATOR "M368")
    target_link_libraries(microbench mp368k_common)
endfunction()
