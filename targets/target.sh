# Rama 68k: lo que tools/build.sh necesita saber de este equipo.
TARGET=68k
TRIPLE=m68k-apple-macos
TOOLCHAIN_NAME=retro68.toolchain.cmake
APPS=(sysprobe microbench decbench decprofile audiotest mp368k)

target_check_app() {
    "$ROOT/targets/check-nofpu.sh" "$1" "$2"
}
