# Synthetic OGG controls

Generated locally with FFmpeg 7.0.2 from sine sources, without user recordings.
These files contain no personal metadata. Reproduce with:

```text
ffmpeg -f lavfi -i sine=frequency=440:sample_rate=22050:duration=0.12 -ac 1 -c:a libvorbis mono22050.ogg
ffmpeg -f lavfi -i sine=frequency=660:sample_rate=44100:duration=0.12 -ac 2 -c:a libvorbis stereo44100.ogg
ffmpeg -f lavfi -i sine=frequency=880:sample_rate=96000:duration=0.12 -ac 2 -c:a libvorbis stereo96000.ogg
ffmpeg -f lavfi -i sine=frequency=440:sample_rate=48000:duration=65 -ac 1 -c:a libvorbis -q:a 0 long48000.ogg
ffmpeg -f lavfi -i sine=frequency=440:sample_rate=48000:duration=0.12 -ac 2 -c:a libopus opus48000.ogg
```

Opus is a non-Vorbis Ogg control. MF support depends on the Windows installation;
the test reports availability and also verifies a successful sniffed WAV fallback.
Malformed inputs are generated in test-owned temporary directories; they are not
shipped as playable fixture assets.
