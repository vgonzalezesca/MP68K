# Rama 68k: lo que tools/build.sh necesita saber de este equipo.
TARGET=68k
TRIPLE=m68k-apple-macos
TOOLCHAIN_NAME=retro68.toolchain.cmake
APPS=(sysprobe microbench decbench decprofile audiotest audiotest_ref codecbench codecbench_ref mp368k segtest qttest)

target_check_app() {
    "$ROOT/targets/check-nofpu.sh" "$1" "$2"
    "$ROOT/targets/check-nosoftfloat.sh" "$1"
}
