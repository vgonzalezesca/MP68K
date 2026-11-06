# MP368K

An MP3 player for 68k Macintosh computers, built for a **Macintosh LC 580** (68LC040 at 33 MHz, **no FPU**, 16 MB RAM) running Mac OS 8.1. It decodes in fixed point with [libmad](https://www.underbit.com/products/mad/) and has a Mac OS 8 Platinum-style interface.

> **Status: work in progress.** Everything has been tested on a PC and in an emulator (Infinite Mac, Quadra 650 ROM, Mac OS 8.1). **Nothing has been measured on a real LC 580 yet**, so there are no valid performance numbers for that machine. Anything unmeasured is marked "unmeasured" in `docs/PERF_LOG.md`.

## Features

- Plays MPEG-1/2 Layer III (CBR and VBR), streaming continuously from disk through the Sound Manager (double buffering).
- Quality profiles: **Full**, **Half Rate**, **Mono**, **Half Rate Mono**, and **Automatic** (picks one based on measured decoding speed).
- Two windows: the player (LCD time display, title, position, volume, balance, transport) and a playlist.
- Playlist with *Add*, *Add Folder* and `.m3u` files; Shuffle and Repeat (Off/All/One).
- ID3v1/v2 tags converted to MacRoman, exact duration from Xing/Info/VBRI headers, and seeking in VBR files.
- Negotiates the output format with the hardware (8- or 16-bit) and keeps playing while another application is in front.
- Application text is in US English.

## How it is built

- **C** with the [Retro68](https://github.com/autc04/Retro68) GCC; 68k assembly only where a measurement justifies it (there are no assembly kernels yet).
- **No FPU:** `-m68040 -msoft-float`, verified on every build with `objdump` (`tools/check-nofpu.sh`).
- **libmad 0.15.1b**, unmodified (`FPM_64BIT`). The Half Rate and Mono profiles are built on its API; profiling uses a patched copy, never the original.
- The decoder and the playlist/tag modules are **portable**: they are tested on the PC with the same sources that are compiled for 68k.
- Project rule: *measure before optimizing*. A performance change is only accepted with a measurement on the LC 580.

## Building

Host requirements (Linux): `cmake gmp mpfr libmpc boost bison flex texinfo ruby`, plus `sox lame ffmpeg python3 numpy` for the tests.

```sh
tools/build-toolchain.sh    # builds Retro68 (68k only), takes 30-60 min
tools/build.sh              # builds every application and checks for FPU instructions
```

Artifacts land in `dist/<app>/` (`.bin` MacBinary, `.dsk` HFS disk image, and `.rsrc`/`.finf` for a Basilisk II shared folder). The player is `dist/mp368k/`.

## Tests (on the PC)

```sh
tools/make_testfiles.sh     # generates test MP3s (CBR, VBR, tagged)
tools/host-ref.sh           # reference decoder and audio checks
tools/host-test.sh          # streaming, ring buffer, gain, ID3, mp3info, play order
```

The audio decoded for 68k is bit-for-bit identical to the PC's (compared by CRC-32).

## Test programs for the real machine

Applications meant to be taken to the LC 580 and measured (`tools/package-visit.sh` assembles `dist/visita1/`):

| App | What it measures |
|---|---|
| `sysprobe` | model, CPU, FPU, Sound Manager, actual output formats and sample rates |
| `microbench` | cost of `MULS.L`, `MOVE16`, `MOVEM`, memory reads and the data cache |
| `decbench` | baseline libmad real-time factor in C, per profile |
| `decprofile` | time breakdown by stage (Huffman, IMDCT, synthesis…) |
| `audiotest` | the complete audio path, and dropouts |

## Layout

```
src/            decoder, audio ring buffer, ID3, mp3info, playlist order
src/common/     Toolbox code: audio (Sound Manager), files, memory, clock
src/player/     application: engine, windows, playlist, resources
bench/          benchmarks without audio
tests/host/     PC tests
third_party/    libmad (GPL) and profiling patches
tools/          build, packaging and test scripts
docs/           decisions (ADRs), stage specs, performance log
```

Main documents (currently in Spanish): [`SPECS.md`](SPECS.md), [`docs/DECISIONS.md`](docs/DECISIONS.md), [`docs/PERF_LOG.md`](docs/PERF_LOG.md), [`docs/UI.md`](docs/UI.md), [`docs/HARDWARE_LC580.md`](docs/HARDWARE_LC580.md).

## License

Personal-use project. Because it includes libmad, the resulting binary is **GPL** (libmad is GPL v2 or later); copyright notices are kept in `third_party/libmad/`. *(A `LICENSE` file has yet to be added to the repository.)*

No Apple ROMs or system images are included.
<img width="808" height="604" alt="image" src="https://github.com/user-attachments/assets/7e7d1c2c-079b-46f1-ace9-142621bbaa5b" />

