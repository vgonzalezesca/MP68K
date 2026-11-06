# Rama 68k: Macintosh LC 580, 68LC040 sin FPU (ver TARGET.md y docs/DECISIONS.md, ADR-005).
set(MP368K_TARGET 68k)
set(MP368K_SYSTEM_NAME Retro68)
set(MP368K_CPU_FLAGS -m68040 -msoft-float)
set(MP368K_OPT_FLAGS -O2 -fomit-frame-pointer -ffunction-sections -fdata-sections)
# Se conservan los nombres para MacsBug (sin --mac-strip-macsbug) para depurar.
set(MP368K_LINK_FLAGS -Wl,-gc-sections)
set(MP368K_APPS sysprobe microbench decbench decprofile audiotest mp368k)

function(mp368k_target_apps)
    # Etapa 1: microbenchmarks de CPU. Mide instrucciones 68k: no aplica en PowerPC.
    add_application(microbench
        targets/microbench.c
        CONSOLE
        CREATOR "M368")
    target_link_libraries(microbench mp368k_common)
endfunction()
