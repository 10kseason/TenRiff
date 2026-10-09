# WASAPI 재생 위치와 관측 시각의 결합

기존 WASAPI 경로는 `GetCurrentPadding`에서 관측한 재생 위치와, `GetBuffer` 이후 `GameSession::audio_callback`에 도착한 시각을 묶었다. 둘 사이에 선점이나 API 지연이 있으면 오래된 재생 위치에 늦은 시각이 붙어 입력 시계와 HUD의 기준이 흔들릴 수 있었다. 이 경로는 1.8.5와 1.8.6에 공통이므로 이번 제보의 원인으로 확정하지 않는다.

`AudioThread::process_buffer`는 이제 padding 조회 직후, `GetBuffer` 전에 QPC를 한 번 읽는다. 기존 `write_cursor - padding` 샘플 값과 4인자 callback 계약을 유지하며, 현재 동기 callback 안에서만 `callback_playback_time_ns()`로 그 관측 시각을 제공한다. callback이 반환하거나 예외로 빠져나오면 RAII로 0을 돌려놓고, 시작·종료에서도 초기화한다. 다음 callback이나 직접 호출이 이전 관측 시각을 재사용하지 않도록 수명을 제한했다.

`GameSession`은 이 시각이 유효할 때 입력의 ClockSync와 startup anchor, HUD의 `audio_sample_time_ns`에 같은 값을 쓴다. ASIO와 직접 호출은 기존처럼 callback에서 QPC를 읽는다. 판정 창·점수·입력 보정량·리플레이 샘플 및 출력 버퍼의 위치는 바꾸지 않는다.

독립 장치 관측에서 `IAudioClock`과 기존 샘플 기준 사이에 약 20ms의 고정 차이가 나타났다. 이 변경은 고정 오프셋까지 바꾸는 장치 시계 교체를 피하고, 관측 이후 전달 지연만 제거한다. padding API에는 대응 QPC가 없으므로 반환 직후를 근사 시각으로 사용한다. API 호출 내부나 반환 직후 QPC를 읽기 전의 선점은 여전히 오차 원인이 될 수 있다.

## 회귀 검증

`tests/unit/test_game_session_audio.cpp`에 다음 검증을 추가했다. 최종 실행 결과는 별도 검증 보고서에 기록한다.

- 같은 샘플/QPC 쌍에 0/1/3/9ms의 추가 전달 지연을 넣고 실제 session callback을 호출한다. ClockSync는 고정하지 않고 callback으로 두 번째 관측점을 학습한다. 입력 기록 48000샘플·판정 delta 0ms·HUD 쌍의 일치를 확인한다. 실제 sleep 길이는 단정하지 않아 스케줄러 지연에 따라 시험이 불안정해지지 않는다.
- 예외가 난 callback 뒤에 시각이 0인지, 직접 session callback이 현재 QPC 범위로 돌아오는지 확인한다.
- ASIO mock의 일반 buffer switch와 time-info switch에서 getter가 0이고 기존 QPC fallback과 HUD 쌍이 유지되는지 확인한다.

이 검증은 입력 이벤트부터 판정/HUD까지의 전달 계약을 검사한다. 실제 키보드와 스피커의 물리 지연, 제보자의 장치, 음성채팅 또는 장시간 플레이의 해결을 입증하지 않는다.
