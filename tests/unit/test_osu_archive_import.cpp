#include "doctest/doctest.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "app/OsuArchiveImport.h"
#include "miniz.h"

namespace {

namespace fs = std::filesystem;

struct TempDirGuard {
    fs::path path;

    ~TempDirGuard() {
        if (!path.empty()) {
            std::error_code ec;
            fs::remove_all(path, ec);
        }
    }
};

struct ZipEntry {
    std::string name;
    std::string content;
};

fs::path make_temp_dir() {
    const fs::path base = fs::temp_directory_path() / "tenriff_osu_archive_import_tests";
    std::error_code ec;
    fs::create_directories(base, ec);
    for (int attempt = 0; attempt < 10000; ++attempt) {
        const fs::path candidate = base / ("case_" + std::to_string(attempt));
        ec.clear();
        if (fs::create_directory(candidate, ec)) {
            return candidate;
        }
    }
    return {};
}

void make_zip(const fs::path& path,
              const std::vector<ZipEntry>& entries,
              bool force_zip64 = false,
              mz_uint compression_level = MZ_NO_COMPRESSION) {
    mz_zip_archive archive{};
    mz_zip_zero_struct(&archive);
    const mz_uint flags = force_zip64 ? MZ_ZIP_FLAG_WRITE_ZIP64 : 0;
    REQUIRE(mz_zip_writer_init_file_v2(&archive, path.string().c_str(), 0, flags) != 0);
    bool finalized = false;
    for (const ZipEntry& entry : entries) {
        const void* data = entry.content.empty() ? static_cast<const void*>("")
                                                  : static_cast<const void*>(entry.content.data());
        if (!mz_zip_writer_add_mem(
                &archive, entry.name.c_str(), data, entry.content.size(), compression_level)) {
            mz_zip_writer_end(&archive);
            CHECK(false);
            return;
        }
    }
    finalized = mz_zip_writer_finalize_archive(&archive) != 0;
    CHECK(mz_zip_writer_end(&archive) != 0);
    REQUIRE(finalized);
}

std::string read_file(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    REQUIRE(input.good());
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

std::vector<std::uint8_t> read_binary(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    REQUIRE(input.good());
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(input),
                                     std::istreambuf_iterator<char>());
}

void write_binary(const fs::path& path, const std::vector<std::uint8_t>& bytes) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    REQUIRE(output.good());
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    REQUIRE(output.good());
}

std::size_t find_last_signature(const std::vector<std::uint8_t>& bytes,
                                std::uint32_t signature) {
    if (bytes.size() < 4) {
        return std::string::npos;
    }
    for (std::size_t offset = bytes.size() - 4;; --offset) {
        const std::uint32_t candidate = static_cast<std::uint32_t>(bytes[offset]) |
                                        (static_cast<std::uint32_t>(bytes[offset + 1]) << 8u) |
                                        (static_cast<std::uint32_t>(bytes[offset + 2]) << 16u) |
                                        (static_cast<std::uint32_t>(bytes[offset + 3]) << 24u);
        if (candidate == signature) {
            return offset;
        }
        if (offset == 0) {
            break;
        }
    }
    return std::string::npos;
}

void write_le16(std::vector<std::uint8_t>& bytes,
                std::size_t offset,
                std::uint16_t value) {
    REQUIRE(offset <= bytes.size());
    REQUIRE(bytes.size() - offset >= 2);
    bytes[offset] = static_cast<std::uint8_t>(value & 0xFFu);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8u);
}

void write_le32(std::vector<std::uint8_t>& bytes,
                std::size_t offset,
                std::uint32_t value) {
    REQUIRE(offset <= bytes.size());
    REQUIRE(bytes.size() - offset >= 4);
    for (unsigned byte = 0; byte < 4; ++byte) {
        bytes[offset + byte] =
            static_cast<std::uint8_t>((value >> (byte * 8u)) & 0xFFu);
    }
}

void write_le64(std::vector<std::uint8_t>& bytes,
                std::size_t offset,
                std::uint64_t value) {
    REQUIRE(offset <= bytes.size());
    REQUIRE(bytes.size() - offset >= 8);
    for (unsigned byte = 0; byte < 8; ++byte) {
        bytes[offset + byte] =
            static_cast<std::uint8_t>((value >> (byte * 8u)) & 0xFFu);
    }
}

void clear_zip_utf8_flags(const fs::path& path) {
    std::vector<std::uint8_t> bytes = read_binary(path);
    for (std::size_t index = 0; index + 10 < bytes.size(); ++index) {
        std::size_t flags_offset = 0;
        if (bytes[index] == 0x50 && bytes[index + 1] == 0x4B && bytes[index + 2] == 0x03 &&
            bytes[index + 3] == 0x04) {
            flags_offset = index + 6;
        } else if (bytes[index] == 0x50 && bytes[index + 1] == 0x4B && bytes[index + 2] == 0x01 &&
                   bytes[index + 3] == 0x02) {
            flags_offset = index + 8;
        } else {
            continue;
        }
        const std::uint16_t flags = static_cast<std::uint16_t>(bytes[flags_offset]) |
                                    (static_cast<std::uint16_t>(bytes[flags_offset + 1]) << 8u);
        const std::uint16_t updated = flags & static_cast<std::uint16_t>(~0x0800u);
        bytes[flags_offset] = static_cast<std::uint8_t>(updated & 0xFFu);
        bytes[flags_offset + 1] = static_cast<std::uint8_t>(updated >> 8u);
    }
    write_binary(path, bytes);
}

void mark_first_entry_as_unix_symlink(const fs::path& path) {
    std::vector<std::uint8_t> bytes = read_binary(path);
    for (std::size_t index = 0; index + 42 < bytes.size(); ++index) {
        if (bytes[index] != 0x50 || bytes[index + 1] != 0x4B || bytes[index + 2] != 0x01 ||
            bytes[index + 3] != 0x02) {
            continue;
        }
        bytes[index + 5] = 3;  // ZIP creator host: Unix.
        const std::uint32_t external_attributes = static_cast<std::uint32_t>(0120777u) << 16u;
        for (unsigned byte = 0; byte < 4; ++byte) {
            bytes[index + 38 + byte] =
                static_cast<std::uint8_t>((external_attributes >> (byte * 8u)) & 0xFFu);
        }
        write_binary(path, bytes);
        return;
    }
    CHECK(false);
}

}  // namespace

TEST_CASE("OSK import unwraps one UTF-8 skin directory and preserves its files") {
    TempDirGuard temp{make_temp_dir()};
    REQUIRE_FALSE(temp.path.empty());
    const fs::path archive = temp.path / "download.osk";
    const std::string skin_name = u8"테스트 스킨";
    const std::string skin_ini = "[General]\nName: Test\n[Mania]\nKeys: 4\n";
    make_zip(archive,
             {{skin_name + "/skin.ini", skin_ini},
              {skin_name + "/mania-note1.png", "PNG"},
              {skin_name + "/효과/빛.png", "LIGHT"}});

    const auto result = tenriff::app::import_osu_archive(
        archive.u8string(), (temp.path / "skins").u8string(), tenriff::app::OsuArchiveKind::Osk);

    CHECK(result.error.empty());
    REQUIRE(result.success);
    CHECK(result.display_name == skin_name);
    CHECK(result.archive_entry_count == 3u);
    CHECK(result.extracted_file_count == 3u);
    CHECK(result.extracted_bytes == skin_ini.size() + 8u);
    const fs::path installed = fs::u8path(result.installed_path);
    CHECK(read_file(installed / "skin.ini").find("Keys: 4") != std::string::npos);
    CHECK(read_file(installed / fs::u8path(u8"효과/빛.png")) == "LIGHT");
    CHECK_FALSE(fs::exists(installed / fs::u8path(skin_name)));
}

TEST_CASE("OSK import extracts deflated entries") {
    TempDirGuard temp{make_temp_dir()};
    REQUIRE_FALSE(temp.path.empty());
    const fs::path archive = temp.path / "deflated.osk";
    const std::string png_payload(16u * 1024u, 'P');
    make_zip(archive,
             {{"skin.ini", "[Mania]\nKeys: 4\n"},
              {"mania-note1.png", png_payload}},
             false,
             MZ_DEFAULT_COMPRESSION);

    const auto result = tenriff::app::import_osu_archive(
        archive.u8string(), (temp.path / "skins").u8string(), tenriff::app::OsuArchiveKind::Osk);

    CHECK(result.error.empty());
    REQUIRE(result.success);
    CHECK(read_file(fs::u8path(result.installed_path) / "mania-note1.png") == png_payload);
}

TEST_CASE("OSK import accepts a normal ZIP64 archive") {
    TempDirGuard temp{make_temp_dir()};
    REQUIRE_FALSE(temp.path.empty());
    const fs::path archive = temp.path / "zip64.osk";
    make_zip(archive,
             {{"skin.ini", "[Mania]\nKeys: 4\n"},
              {"mania-note1.png", "PNG"}},
             true);

    const auto result = tenriff::app::import_osu_archive(
        archive.u8string(), (temp.path / "skins").u8string(), tenriff::app::OsuArchiveKind::Osk);

    CHECK(result.error.empty());
    REQUIRE(result.success);
    CHECK(result.archive_entry_count == 2u);
    CHECK(read_file(fs::u8path(result.installed_path) / "mania-note1.png") == "PNG");
}

TEST_CASE("archive import rejects forged classic and ZIP64 entry counts before extraction") {
    TempDirGuard temp{make_temp_dir()};
    REQUIRE_FALSE(temp.path.empty());

    const fs::path classic = temp.path / "classic-count.osk";
    make_zip(classic, {{"skin.ini", "[Mania]\nKeys: 4\n"}});
    std::vector<std::uint8_t> bytes = read_binary(classic);
    std::size_t eocd = find_last_signature(bytes, 0x06054B50u);
    REQUIRE(eocd != std::string::npos);
    write_le16(bytes, eocd + 8, 60000);
    write_le16(bytes, eocd + 10, 60000);
    write_binary(classic, bytes);

    auto result = tenriff::app::import_osu_archive(
        classic.u8string(),
        (temp.path / "classic-destination").u8string(),
        tenriff::app::OsuArchiveKind::Osk);
    CHECK_FALSE(result.success);
    CHECK(result.error.find("entry-count limit") != std::string::npos);
    CHECK_FALSE(fs::exists(temp.path / "classic-destination"));

    const fs::path zip64 = temp.path / "zip64-count.osk";
    make_zip(zip64, {{"skin.ini", "[Mania]\nKeys: 4\n"}}, true);
    bytes = read_binary(zip64);
    const std::size_t zip64_eocd = find_last_signature(bytes, 0x06064B50u);
    REQUIRE(zip64_eocd != std::string::npos);
    write_le64(bytes, zip64_eocd + 24, 50000);
    write_le64(bytes, zip64_eocd + 32, 50000);
    write_binary(zip64, bytes);

    result = tenriff::app::import_osu_archive(
        zip64.u8string(),
        (temp.path / "zip64-destination").u8string(),
        tenriff::app::OsuArchiveKind::Osk);
    CHECK_FALSE(result.success);
    CHECK(result.error.find("entry-count limit") != std::string::npos);
    CHECK_FALSE(fs::exists(temp.path / "zip64-destination"));
}

TEST_CASE("archive import rejects forged central-directory size and offset before extraction") {
    TempDirGuard temp{make_temp_dir()};
    REQUIRE_FALSE(temp.path.empty());
    const std::vector<ZipEntry> entries = {{"skin.ini", "[Mania]\nKeys: 4\n"}};

    const fs::path huge_size = temp.path / "central-size.osk";
    make_zip(huge_size, entries);
    std::vector<std::uint8_t> bytes = read_binary(huge_size);
    std::size_t eocd = find_last_signature(bytes, 0x06054B50u);
    REQUIRE(eocd != std::string::npos);
    write_le32(bytes, eocd + 12, 0x7FFFFFF0u);
    write_binary(huge_size, bytes);

    auto result = tenriff::app::import_osu_archive(
        huge_size.u8string(),
        (temp.path / "size-destination").u8string(),
        tenriff::app::OsuArchiveKind::Osk);
    CHECK_FALSE(result.success);
    CHECK(result.error.find("bounded metadata allowance") != std::string::npos);
    CHECK_FALSE(fs::exists(temp.path / "size-destination"));

    const fs::path huge_offset = temp.path / "central-offset.osk";
    make_zip(huge_offset, entries);
    bytes = read_binary(huge_offset);
    eocd = find_last_signature(bytes, 0x06054B50u);
    REQUIRE(eocd != std::string::npos);
    write_le32(bytes, eocd + 16, 0xFFFFFF00u);
    write_binary(huge_offset, bytes);

    result = tenriff::app::import_osu_archive(
        huge_offset.u8string(),
        (temp.path / "offset-destination").u8string(),
        tenriff::app::OsuArchiveKind::Osk);
    CHECK_FALSE(result.success);
    CHECK(result.error.find("outside the archive") != std::string::npos);
    CHECK_FALSE(fs::exists(temp.path / "offset-destination"));
}

TEST_CASE("archive import rejects multi-disk EOCD metadata before extraction") {
    TempDirGuard temp{make_temp_dir()};
    REQUIRE_FALSE(temp.path.empty());
    const fs::path archive = temp.path / "multi-disk.osk";
    make_zip(archive, {{"skin.ini", "[Mania]\nKeys: 4\n"}});
    std::vector<std::uint8_t> bytes = read_binary(archive);
    const std::size_t eocd = find_last_signature(bytes, 0x06054B50u);
    REQUIRE(eocd != std::string::npos);
    write_le16(bytes, eocd + 4, 1);
    write_binary(archive, bytes);

    const auto result = tenriff::app::import_osu_archive(
        archive.u8string(),
        (temp.path / "skins").u8string(),
        tenriff::app::OsuArchiveKind::Osk);

    CHECK_FALSE(result.success);
    CHECK(result.error.find("multi-disk") != std::string::npos);
    CHECK_FALSE(fs::exists(temp.path / "skins"));
}

TEST_CASE("OSK import never overwrites an existing skin") {
    TempDirGuard temp{make_temp_dir()};
    REQUIRE_FALSE(temp.path.empty());
    const fs::path archive = temp.path / "Same.osk";
    const fs::path destination = temp.path / "skins";
    make_zip(archive, {{"skin.ini", "[Mania]\nKeys: 4\n"}, {"mania-note1.png", "ONE"}});

    const auto first = tenriff::app::import_osu_archive(
        archive.u8string(), destination.u8string(), tenriff::app::OsuArchiveKind::Osk);
    CHECK(first.error.empty());
    REQUIRE(first.success);
    const auto second = tenriff::app::import_osu_archive(
        archive.u8string(), destination.u8string(), tenriff::app::OsuArchiveKind::Osk);

    CHECK(second.error.empty());
    REQUIRE(second.success);
    CHECK(first.display_name == "Same");
    CHECK(second.display_name == "Same (2)");
    CHECK(read_file(fs::u8path(first.installed_path) / "mania-note1.png") == "ONE");
    CHECK(read_file(fs::u8path(second.installed_path) / "mania-note1.png") == "ONE");
}

#ifdef _WIN32
TEST_CASE("OSK import decodes legacy CP932 names when the ZIP UTF-8 flag is absent") {
    TempDirGuard temp{make_temp_dir()};
    REQUIRE_FALSE(temp.path.empty());
    const fs::path archive = temp.path / "legacy.osk";
    const std::string cp932_name = std::string("\x83\x58\x83\x4C\x83\x93", 6);
    make_zip(archive,
             {{cp932_name + "/skin.ini", "[Mania]\nKeys: 4\n"},
              {cp932_name + "/mania-note1.png", "PNG"}});
    clear_zip_utf8_flags(archive);

    const auto result = tenriff::app::import_osu_archive(
        archive.u8string(), (temp.path / "skins").u8string(), tenriff::app::OsuArchiveKind::Osk);

    CHECK(result.error.empty());
    REQUIRE(result.success);
    CHECK(result.display_name == u8"スキン");
    CHECK(fs::exists(fs::u8path(result.installed_path) / "skin.ini"));
}
#endif

TEST_CASE("OSZ import requires an osu file and preserves the complete beatmap payload") {
    TempDirGuard temp{make_temp_dir()};
    REQUIRE_FALSE(temp.path.empty());
    const fs::path archive = temp.path / "Artist - Song.osz";
    make_zip(archive,
             {{"Map/Hard.osu", "osu file format v14\n[General]\nMode: 3\n"},
              {"Map/audio.ogg", "OGG"},
              {"Map/bg/image.jpg", "JPEG"}});

    const auto result = tenriff::app::import_osu_archive(
        archive.u8string(), (temp.path / "songs").u8string(), tenriff::app::OsuArchiveKind::Osz);

    CHECK(result.error.empty());
    REQUIRE(result.success);
    CHECK(result.display_name == "Map");
    const fs::path installed = fs::u8path(result.installed_path);
    CHECK(read_file(installed / "Hard.osu").find("Mode: 3") != std::string::npos);
    CHECK(read_file(installed / "audio.ogg") == "OGG");
    CHECK(read_file(installed / "bg" / "image.jpg") == "JPEG");
}

TEST_CASE("archive import rejects traversal before creating a destination") {
    TempDirGuard temp{make_temp_dir()};
    REQUIRE_FALSE(temp.path.empty());
    const fs::path archive = temp.path / "escape.osk";
    const fs::path destination = temp.path / "skins";
    make_zip(archive, {{"skin.ini", "[Mania]\nKeys: 4\n"}, {"../escaped.txt", "BAD"}});

    const auto result = tenriff::app::import_osu_archive(
        archive.u8string(), destination.u8string(), tenriff::app::OsuArchiveKind::Osk);

    CHECK_FALSE(result.success);
    CHECK(result.error.find("relative path") != std::string::npos);
    CHECK_FALSE(fs::exists(destination));
    CHECK_FALSE(fs::exists(temp.path / "escaped.txt"));
}

TEST_CASE("archive import rejects case-fold duplicates and prefix collisions") {
    TempDirGuard temp{make_temp_dir()};
    REQUIRE_FALSE(temp.path.empty());
    const fs::path duplicate = temp.path / "duplicate.osk";
    make_zip(duplicate,
             {{"skin.ini", "[Mania]\nKeys: 4\n"}, {"Asset.png", "A"}, {"asset.PNG", "B"}});
    auto result = tenriff::app::import_osu_archive(
        duplicate.u8string(), (temp.path / "skins-a").u8string(), tenriff::app::OsuArchiveKind::Osk);
    CHECK_FALSE(result.success);
    CHECK(result.error.find("duplicate") != std::string::npos);

    const fs::path prefix = temp.path / "prefix.osk";
    make_zip(prefix,
             {{"skin.ini", "[Mania]\nKeys: 4\n"}, {"asset", "FILE"}, {"asset/note.png", "PNG"}});
    result = tenriff::app::import_osu_archive(
        prefix.u8string(), (temp.path / "skins-b").u8string(), tenriff::app::OsuArchiveKind::Osk);
    CHECK_FALSE(result.success);
    CHECK(result.error.find("prefix collision") != std::string::npos);
}

TEST_CASE("archive import rejects symbolic links and Windows-reserved paths during preflight") {
    TempDirGuard temp{make_temp_dir()};
    REQUIRE_FALSE(temp.path.empty());
    const fs::path symlink_archive = temp.path / "symlink.osk";
    make_zip(symlink_archive, {{"skin.ini", "target"}, {"mania-note1.png", "PNG"}});
    mark_first_entry_as_unix_symlink(symlink_archive);

    auto result = tenriff::app::import_osu_archive(
        symlink_archive.u8string(),
        (temp.path / "skins-a").u8string(),
        tenriff::app::OsuArchiveKind::Osk);
    CHECK_FALSE(result.success);
    CHECK(result.error.find("symbolic link") != std::string::npos);
    CHECK_FALSE(fs::exists(temp.path / "skins-a"));

    const fs::path reserved_archive = temp.path / "reserved.osk";
    make_zip(reserved_archive,
             {{"skin.ini", "[Mania]\nKeys: 4\n"}, {"CON.txt", "reserved"}});
    result = tenriff::app::import_osu_archive(
        reserved_archive.u8string(),
        (temp.path / "skins-b").u8string(),
        tenriff::app::OsuArchiveKind::Osk);
    CHECK_FALSE(result.success);
    CHECK(result.error.find("reserved Windows") != std::string::npos);
    CHECK_FALSE(fs::exists(temp.path / "skins-b"));
}

TEST_CASE("archive import applies strict kind and semantic validation") {
    TempDirGuard temp{make_temp_dir()};
    REQUIRE_FALSE(temp.path.empty());
    const fs::path wrong_extension = temp.path / "skin.zip";
    make_zip(wrong_extension, {{"skin.ini", "[Mania]\nKeys: 4\n"}});
    auto result = tenriff::app::import_osu_archive(
        wrong_extension.u8string(), (temp.path / "skins-a").u8string(), tenriff::app::OsuArchiveKind::Osk);
    CHECK_FALSE(result.success);
    CHECK(result.error.find("extension") != std::string::npos);

    const fs::path no_skin = temp.path / "not-a-skin.osk";
    make_zip(no_skin, {{"readme.txt", "nothing to import"}});
    result = tenriff::app::import_osu_archive(
        no_skin.u8string(), (temp.path / "skins-b").u8string(), tenriff::app::OsuArchiveKind::Osk);
    CHECK_FALSE(result.success);
    CHECK(result.error.find("skin root") != std::string::npos);

    const fs::path no_chart = temp.path / "not-a-map.osz";
    make_zip(no_chart, {{"audio.ogg", "audio only"}});
    result = tenriff::app::import_osu_archive(
        no_chart.u8string(), (temp.path / "songs").u8string(), tenriff::app::OsuArchiveKind::Osz);
    CHECK_FALSE(result.success);
    CHECK(result.error.find(".osu") != std::string::npos);
}

TEST_CASE("archive import enforces configured expansion limits") {
    TempDirGuard temp{make_temp_dir()};
    REQUIRE_FALSE(temp.path.empty());
    const fs::path archive = temp.path / "large.osk";
    make_zip(archive, {{"skin.ini", "[Mania]\nKeys: 4\n"}, {"large.bin", std::string(32, 'x')}});
    tenriff::app::OsuArchiveImportLimits limits;
    limits.max_total_uncompressed_bytes = 16;

    const auto result = tenriff::app::import_osu_archive(
        archive.u8string(), (temp.path / "skins").u8string(), tenriff::app::OsuArchiveKind::Osk, limits);

    CHECK_FALSE(result.success);
    CHECK(result.error.find("size limits") != std::string::npos);
    CHECK_FALSE(fs::exists(temp.path / "skins"));
}

TEST_CASE("archive import detects corrupt file data by CRC and removes its own staging directory") {
    TempDirGuard temp{make_temp_dir()};
    REQUIRE_FALSE(temp.path.empty());
    const fs::path archive = temp.path / "corrupt.osk";
    const std::string marker = "UNIQUE-CRC-CONTENT";
    make_zip(archive,
             {{"skin.ini", "[Mania]\nKeys: 4\n"},
              {"mania-note1.png", marker}});

    std::vector<std::uint8_t> bytes = read_binary(archive);
    const auto found = std::search(bytes.begin(), bytes.end(), marker.begin(), marker.end());
    REQUIRE(found != bytes.end());
    *found ^= 0x01u;
    write_binary(archive, bytes);

    const fs::path destination = temp.path / "skins";
    const auto result = tenriff::app::import_osu_archive(
        archive.u8string(), destination.u8string(), tenriff::app::OsuArchiveKind::Osk);

    CHECK_FALSE(result.success);
    CHECK(result.error.find("extraction failed") != std::string::npos);
    const fs::path staging = destination / ".tenriff" / "import-staging";
    std::error_code ec;
    const auto begin = fs::directory_iterator(staging, ec);
    CHECK((ec || begin == fs::directory_iterator{}));
}
