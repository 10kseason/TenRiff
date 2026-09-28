#include "doctest/doctest.h"

#include "ui/Localization.h"

using tenriff::ui::Language;
using tenriff::ui::cycle_language;
using tenriff::ui::language_from_token;
using tenriff::ui::language_token;
using tenriff::ui::text;

TEST_CASE("UI language normalization accepts Japanese without changing legacy tokens") {
    CHECK(language_from_token("ja") == Language::Japanese);
    CHECK(language_from_token("JA-jp") == Language::Japanese);
    CHECK(language_from_token("ja_JP") == Language::Japanese);
    CHECK(language_from_token("JP") == Language::Japanese);
    CHECK(language_from_token("Japanese") == Language::Japanese);
    CHECK(language_from_token("ko-KR") == Language::Korean);
    CHECK(language_from_token("kr") == Language::Korean);
    CHECK(language_from_token("en") == Language::English);
    CHECK(language_from_token("unknown") == Language::English);
    CHECK(language_token(Language::Japanese) == "ja");
}

TEST_CASE("UI language cycles all three choices in both directions") {
    CHECK(cycle_language(Language::English, 1) == Language::Korean);
    CHECK(cycle_language(Language::Korean, 1) == Language::Japanese);
    CHECK(cycle_language(Language::Japanese, 1) == Language::English);
    CHECK(cycle_language(Language::English, -1) == Language::Japanese);
    CHECK(cycle_language(Language::Japanese, -1) == Language::Korean);
    CHECK(cycle_language(Language::Korean, -1) == Language::English);
    CHECK(cycle_language(Language::Japanese, 0) == Language::Japanese);
}

TEST_CASE("Japanese UI covers menu settings loading result and BMS editing") {
    CHECK(text(Language::Japanese, "PLAY", "플레이") == "プレイ");
    CHECK(text(Language::Japanese, "Language", "언어") == "言語");
    CHECK(text(Language::Japanese, "Japanese", "일본어") == "日本語");
    CHECK(text(Language::Japanese, "Loading profile", "Loading profile") == "プロファイルを読み込み中");
    CHECK(text(Language::Japanese, "HARD CLEAR", "하드 클리어") == "ハードクリア");
    CHECK(text(Language::Japanese, "MAX COMBO", "최대 콤보") == "最大コンボ");
    CHECK(text(Language::Japanese, "Note added", "Note added") == "ノートを追加しました");
    CHECK(text(Language::Japanese, "Open Skin Editor", "스킨 에디터 열기") == "スキンエディターを開く");
}

TEST_CASE("Localization retains English Korean and unknown user values verbatim") {
    CHECK(text(Language::English, "PLAY", "플레이") == "PLAY");
    CHECK(text(Language::Korean, "PLAY", "플레이") == "플레이");
    CHECK(text(Language::Korean, "not in catalog", "사용자 정의") == "사용자 정의");
    CHECK(text(Language::Japanese, "my-chart-曲名.bms", "변경 금지") == "my-chart-曲名.bms");
    CHECK(text(Language::Japanese, "", "") == "");
    CHECK(text(Language::Japanese, "REPLAY ", "리플레이 ") == "リプレイ ");
    CHECK(text(Language::Japanese, "\xE2\x96\xB6  WATCH REPLAY", "리플레이 보기") == "▶  リプレイを見る");
}
