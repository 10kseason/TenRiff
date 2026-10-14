#pragma once
#include <string>

// Synthetic reproduction: coarse STOP storage sorts before a coincident BPM
// reset, then an extreme BPM burst carries a mine just after a playable tap.
inline std::string bpm_stop_mine_fixture() {
    std::string bpm(256, '0'), reset(256, '0');
    bpm.replace(0, 2, "01");
    bpm.replace(130, 2, "01");
    reset.replace(128, 2, "BA");
    return "#TITLE BPM STOP Mine Regression\n#BPM 186\n#5K\n#RANK 3\n"
        "#BPM01 65535.9999\n#STOP01 48\n#00012:01\n#00008:" + bpm +
        "\n#00003:" + reset + "\n#00009:0001\n#000D2:0000000000ZX0000\n#00111:01\n";
}
