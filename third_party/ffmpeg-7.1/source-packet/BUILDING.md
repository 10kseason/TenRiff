# FFmpeg 7.1 minimal raw-image filter runtime

This package uses FFmpeg under the GNU Lesser General Public License version 2.1 or later. See `COPYING.LGPLv2.1.txt` and the upstream `LICENSE.md`. FFmpeg is a separate command-line executable; TenRiff exchanges raw pixels with it through process pipes and does not link its libraries.

The unmodified corresponding source is `ffmpeg-7.1.tar.xz`, downloaded from https://ffmpeg.org/releases/ffmpeg-7.1.tar.xz. SHA-256: `40973d44970dbc83ef302b0609f2e74982be2d85916dd2ee7472d30678a7abe6`. The exact source archive, license notices and build script accompany this binary in the TenRiff source package. No AI model is included in this FFmpeg package.

Build on Windows x64 with MSYS2 MinGW-w64 GCC 15.1.0 and GNU Make. Extract the archive beside `build-minimal.sh`, open a MINGW64 shell in that directory, and run `bash build-minimal.sh`. The script contains the complete configure flags and creates `ffmpeg-7.1/ffmpeg.exe`. It uses an in-tree build because FFmpeg 7.1 does not support out-of-tree source paths containing spaces. No upstream source changes are required. This is a build recipe, not a claim of bit-for-bit reproducibility with other toolchain versions.

The build disables GPL/nonfree code, network, external-library autodetection, documentation, debug symbols, standalone assembly, FFplay and FFprobe. It enables only FFmpeg's required command-line internals, rawvideo decoding/encoding/demuxing/muxing, pipe protocol, and the format/scale/deblock filter chain. The CLI's own FFmpeg libraries are statically linked into the standalone executable.

Supported command:

```text
ffmpeg -hide_banner -v error -nostdin -threads 1 -filter_threads 1 -f rawvideo -pixel_format bgra -video_size WIDTHxHEIGHT -i pipe:0 -vf format=gbrp,deblock=filter=weak:block=8:planes=7,format=bgra -frames:v 1 -an -sn -dn -threads 1 -pix_fmt bgra -f rawvideo pipe:1
```

The planar RGB conversion discards alpha; TenRiff restores source alpha exactly after reading the filtered RGB and before AI/base composition. Install this limited-purpose executable at `tools/ffmpeg-deblock/ffmpeg.exe` beside TenRiff, not as a replacement for a general-purpose FFmpeg installation. Other codecs and media containers are intentionally unavailable.

Source and project information: https://ffmpeg.org/ ; license information: https://ffmpeg.org/legal.html . No additional restrictions on use, modification, replacement or reverse engineering are imposed by this package.
