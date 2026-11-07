# MP368K

An audio player for classic Mac OS. It started as an MP3 player for a **Macintosh LC 580** (68LC040 at 33 MHz, **no FPU**, 16 MB RAM) running Mac OS 8.1, and it is forking into a 68k version and a PowerPC version. It decodes in fixed point with [libmad](https://www.underbit.com/products/mad/) and has a Mac OS 8 Platinum-style interface.

> **Status: work in progress.** The 68k MP3 player has been tested on a PC and in an emulator (Infinite Mac, Quadra 650 ROM, Mac OS 8.1). **Nothing has been measured on real hardware yet**, so there are no valid performance numbers for any machine. Anything unmeasured is marked "sin medir" (unmeasured) in `docs/PERF_LOG.md`.

## Branches

| Branch | Target | Language | What is there today |
|---|---|---|---|
| `master` | shared code and documents | portable C | builds PC tests only |
| `68k` | Macintosh LC 580 (68LC040) | C, plus 68040 assembly for decoding kernels (not written yet) | the full MP3 player |
| `ppc` | Power Macintosh with a PowerPC 601 | C only | `sysprobe` only; the player has not been ported yet |

Three builds come out of them: **68k**, **PowerPC** and **FAT** (one file with both). The FAT currently contains `sysprobe` only, and has not been run on either side yet.

**Done on the `68k` branch** ([`docs/ETAPA6.md`](docs/ETAPA6.md)): FLAC (checked in an emulator against the PC, bit for bit), a 10-band equalizer for MP3, Ogg Vorbis (Tremor) and WMA v1/v2 (libwma), a hand-drawn Platinum icon, and 68040 assembly kernels for the MP3 decoder (polyphase window, DCT32, long-block IMDCT, PCM conversion). Vorbis and WMA match `ffmpeg` to within 1 LSB, and the assembly kernels give the same PCM as the C code, all verified on a PC and under `qemu-m68k` (big-endian); **none of it has been run on a real Mac, and nothing has been timed on the LC 580.** **Planned, not implemented:** the PowerPC port, other formats through QuickTime, assembly for Vorbis/WMA, and the equalizer for Vorbis/WMA. Whether Ogg and WMA can reach real time on a 33 MHz 68LC040 is unknown.

## Features

These describe the MP3 player on the `68k` branch.

- Plays MPEG-1/2 Layer III (CBR and VBR), streaming continuously from disk through the Sound Manager (double buffering).
- Quality profiles: **Full**, **Half Rate**, **Mono**, **Half Rate Mono**, and **Automatic** (picks one based on measured decoding speed).
- Two windows: the player (LCD time display, title, position, volume, balance, transport) and a playlist.
- Playlist with *Add*, *Add Folder* and `.m3u` files; Shuffle and Repeat (Off/All/One).
- ID3v1/v2 tags converted to MacRoman, exact duration from Xing/Info/VBRI headers, and seeking in VBR files.
- Negotiates the output format with the hardware (8- or 16-bit) and keeps playing while another application is in front.
- Application text is in US English.

## How it is built

- **C** with the [Retro68](https://github.com/autc04/Retro68) GCC; 68k assembly only where a measurement justifies it (there are no assembly kernels yet).
- **No FPU on 68k:** `-m68040 -msoft-float`, verified on every build with `objdump` (`targets/check-nofpu.sh`, on the `68k` branch).
- **libmad 0.15.1b**, unmodified (`FPM_64BIT`). The Half Rate and Mono profiles are built on its API; profiling uses a patched copy, never the original.
- The decoder and the playlist/tag modules are **portable**: they are tested on the PC with the same sources that are compiled for 68k.
- Project rule: *no speed claim without a measurement*. Assembly may be written before measuring, but each kernel keeps a C reference, a bit-exact test and a C/assembly build switch, and it is not called faster until it has been measured on the real machine.

## Building

Host requirements (Linux): `cmake gmp mpfr libmpc boost bison flex texinfo ruby`, plus `sox lame ffmpeg python3 numpy` for the tests.

```sh
tools/build-toolchain.sh    # builds Retro68 (68k and classic PowerPC); 11 min on 12 threads
git switch 68k              # or ppc; master does not build Mac applications
tools/build.sh              # builds this branch and runs its checks
tools/build-all.sh          # the three builds, from the committed 68k and ppc branches
```

Artifacts land in `dist/<target>/<app>/`, where the target is `68k`, `ppc` or `fat` (`.bin` MacBinary, `.dsk` HFS disk image, and `.rsrc`/`.finf` for a Basilisk II shared folder). The player is `dist/68k/mp368k/`.

## Tests (on the PC)

```sh
tools/make_testfiles.sh     # generates test MP3s (CBR, VBR, tagged)
tools/host-ref.sh           # reference decoder and audio checks
tools/host-test.sh          # streaming, ring buffer, gain, ID3, mp3info, play order
```

The audio decoded for 68k is bit-for-bit identical to the PC's (compared by CRC-32).

## Test programs for the real machine

Applications meant to be taken to the LC 580 and measured (on the `68k` branch, `targets/package-visit.sh` assembles `dist/visita1/`):

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
tools/          build and test scripts
docs/           decisions (ADRs), stage specs, performance log
targets/        68k and ppc branches only: flags, clock, hooks, target-specific code
```

Main documents (currently in Spanish): [`SPECS.md`](SPECS.md), [`docs/SPECS_68K.md`](docs/SPECS_68K.md), [`docs/SPECS_PPC.md`](docs/SPECS_PPC.md), [`docs/ETAPA6.md`](docs/ETAPA6.md), [`docs/DECISIONS.md`](docs/DECISIONS.md), [`docs/PERF_LOG.md`](docs/PERF_LOG.md), [`docs/UI.md`](docs/UI.md), [`docs/HARDWARE_LC580.md`](docs/HARDWARE_LC580.md), [`docs/HARDWARE_PPC601.md`](docs/HARDWARE_PPC601.md).

## License

Personal-use project, released under the **GPL v3** ([`LICENSE`](LICENSE)). It includes libmad, which is GPL v2 or later; copyright notices are kept in `third_party/libmad/`.

No Apple ROMs or system images are included.
<img width="808" height="604" alt="image" src="https://github.com/user-attachments/assets/7e7d1c2c-079b-46f1-ace9-142621bbaa5b" />
<img width="808" height="604" alt="image" src="https://github.com/user-attachments/assets/00421371-f20a-4624-a225-91c361f6fa19" />
<img width="808" height="604" alt="image" src="https://github.com/user-attachments/assets/c4179492-f44c-4694-9912-44472e3a1bae" />


