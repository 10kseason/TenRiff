# Native ASIO Output

TenRiff 1.7.2 adds a native Windows ASIO output backend alongside WASAPI. WASAPI remains the default, so existing profiles keep their output choice until the user selects ASIO. The backend applies to **gameplay audio and song preview**. Main-menu/result background music retains its separate Windows MCI playback path.

ASIO needs a vendor-installed **64-bit Windows ASIO driver**. TenRiff uses the driver's native callbacks and the first stereo output pair. It does not install an audio driver, include the Steinberg SDK, or claim driver certification. The implementation uses the existing TenRiff mixer through a directly implemented Windows driver interface.

## 설정 방법

1. 오디오 장치 제조사의 64비트 ASIO 드라이버를 설치합니다. TenRiff는 설치된 드라이버만 목록에 표시합니다.
2. `Options > Audio`의 **Audio Backend**를 **ASIO**로 바꿉니다.
3. **ASIO Driver**에서 장치를 선택합니다. **Auto**는 이름순 목록의 첫 드라이버를 사용합니다. **F5**는 목록을 새로 읽습니다.
4. **ASIO Sample Rate**와 **ASIO Buffer**를 선택하고 **Back**으로 저장·적용합니다. 장치가 지원하는 샘플레이트여야 하며 실제 버퍼 프레임은 드라이버의 허용 규칙에 맞춰 결정됩니다.
5. 드라이버를 사용할 수 없거나 재생 중 설정이 바뀌면 오류를 표시하고 선곡으로 돌아갑니다. WASAPI로 자동 전환하지 않으므로 Audio 화면에서 드라이버를 다시 선택하거나 WASAPI를 명시적으로 선택합니다.

메뉴 음악이 들리는 것만으로 ASIO 출력이 확인되지는 않습니다. 실제 곡 미리듣기 또는 플레이 오디오는 첫 스테레오 출력 쌍을 사용합니다. 채널 라우팅과 장치 제어판 설정은 제조사 도구에서 조정합니다.

## Saved settings

| Field | Behavior |
| --- | --- |
| `audio.backend` | `wasapi` (default) or `asio`. |
| `audio.asio_driver` | Registered driver CLSID; empty means Auto. Device names are shown in the UI. |
| `audio.rate` | Requested sample rate, held fixed for ASIO. The UI offers 44.1, 48, 88.2, 96, 176.4 and 192 kHz. Chart audio is resampled to it. |
| `audio.frames` | Requested callback frames. The UI offers 32, 64, 128, 256, 512, 1024 and 2048. Driver negotiation may select a different legal value. |

WASAPI's exclusive/shared mode and period count do not select an ASIO mode. The audio preset leaves ASIO's saved frame count unchanged. ASIO sample-rate selection must succeed and read back at the requested rate; there is no implicit sample-rate fallback.

## Driver boundary

The Windows x64 registry supplies the driver CLSID. Driver loading and lifecycle calls run on a dedicated COM control thread; the driver's callback directly renders the existing interleaved stereo mixer into its first two output channels. Only one ASIO stream can be open at once.

Supported channel formats are Int16, packed Int24, Int32, Float32 and Float64 in either endian order, plus the Int32 formats with 16/18/20/24 valid bits. DSD output is rejected.

Buffer negotiation uses the driver's preferred size when granularity is zero, the closest allowed power of two for granularity -1, or the closest legal increment for positive granularity. Equal-distance choices select the larger buffer. The backend reports the negotiated frame count rather than assuming the request was accepted.

Playback time uses the normalized driver sample position minus reported output latency. If the driver cannot provide a sample position, the backend maintains a monotonic write-cursor fallback. This is a scheduling clock, not an independently measured hardware-latency result.

Initialization/start failures are explicit. Runtime reset, resync, latency, buffer or sample-rate changes silence the stream and report an error requiring reopening; the callback never tears down the driver. The failed gameplay session does not export a normal result or replay. Driver replacement, runtime recovery and audible behavior should be verified on the actual device.

Device setup and callback smoke results are recorded separately in the [1.7.2 release gate](release-1.7.2-gate.md). A successful silent callback test is not a measurement of audible quality, keyboard-to-audio latency or long-session stability.

### 1.7.2 device verification limitation

The test PC's registered Realtek ASIO driver opened, reported an active internal clock and created buffers, but reported a current sample rate of zero and returned `-1000 (NotPresent)` for both 44.1 and 48 kHz setters. The same result occurred with an application-owned hidden window and a null host window. Native playback could not start; this is a failed device smoke, not a successful hardware output test. TenRiff preserves this failure and does not guess a clock rate or switch to WASAPI. Native protocol behavior is covered by mock-driver tests; audible output and compatibility with operational vendor devices remain unverified in this release.

Drivers already reporting the exact requested rate do not need a setter. A requested rate change must still succeed and read back exactly; a zero or invalid readback never qualifies.
