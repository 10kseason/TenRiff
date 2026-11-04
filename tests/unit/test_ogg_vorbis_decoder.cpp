#include "doctest/doctest.h"

#ifdef _WIN32

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include "audio/OggVorbisDecoder.h"
#include "app/SongPreviewBuilder.h"
#include "app/BmsEditorPreview.h"
#include "app/AudioFileDecoder.h"
#include "../support/OggTestFixture.h"
#include <chrono>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace {

std::vector<unsigned char> decode_base64(std::string_view input) {
    auto decode_char = [](unsigned char ch) -> int {
        if (ch >= 'A' && ch <= 'Z') {
            return ch - 'A';
        }
        if (ch >= 'a' && ch <= 'z') {
            return ch - 'a' + 26;
        }
        if (ch >= '0' && ch <= '9') {
            return ch - '0' + 52;
        }
        if (ch == '+') {
            return 62;
        }
        if (ch == '/') {
            return 63;
        }
        return -1;
    };

    std::vector<unsigned char> output;
    unsigned int val = 0;
    int valb = -8;
    for (unsigned char ch : input) {
        if (std::isspace(ch) || ch == '=') {
            continue;
        }
        const int decoded = decode_char(ch);
        if (decoded < 0) {
            continue;
        }
        val = (val << 6) + decoded;
        valb += 6;
        if (valb >= 0) {
            output.push_back(static_cast<unsigned char>((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return output;
}

const char* kTinyOggBase64 =
    "T2dnUwACAAAAAAAAAAAeT756AAAAABBr03oBHgF2b3JiaXMAAAAAAYC7AAAAAAAAgDgBAAAAAAC4AU9nZ1MAAAAAAAAAAAAA"
    "Hk++egEAAADGluahDj7///////////////+BA3ZvcmJpcwwAAABMYXZmNjEuMS4xMDABAAAAHgAAAGVuY29kZXI9TGF2YzYx"
    "LjMuMTAwIGxpYnZvcmJpcwEFdm9yYmlzIkJDVgEAQAAAJHMYKkalcxaEEBpCUBnjHELOa+wZQkwRghwyTFvLJXOQIaSgQohb"
    "KIHQkFUAAEAAAIdBeBSEikEIIYQlPViSgyc9CCGEiDl4FIRpQQghhBBCCCGEEEIIIYRFOWiSgydBCB2E4zA4DIPlOPgchEU5"
    "WBCDJ0HoIIQPQriag6w5CCGEJDVIUIMGOegchMIsKIqCxDC4FoQENSiMguQwyNSDC0KImoNJNfgahGdBeBaEaUEIIYQkQUiQ"
    "gwZByBiERkFYkoMGObgUhMtBqBqEKjkIH4QgNGQVAJAAAKCiKIqiKAoQGrIKAMgAABBAURTHcRzJkRzJsRwLCA1ZBQAAAQAI"
    "AACgSIqkSI7kSJIkWZIlWZIlWZLmiaosy7Isy7IsyzIQGrIKAEgAAFBRDEVxFAcIDVkFAGQAAAigOIqlWIqlaIrniI4IhIas"
    "AgCAAAAEAAAQNENTPEeURM9UVde2bdu2bdu2bdu2bdu2bVuWZRkIDVkFAEAAABDSaWapBogwAxkGQkNWAQAIAACAEYowxIDQ"
    "kFUAAEAAAIAYSg6iCa0535zjoFkOmkqxOR2cSLV5kpuKuTnnnHPOyeacMc4555yinFkMmgmtOeecxKBZCpoJrTnnnCexedCa"
    "Kq0555xxzulgnBHGOeecJq15kJqNtTnnnAWtaY6aS7E555xIuXlSm0u1Oeecc84555xzzjnnnOrF6RycE84555yovbmWm9DF"
    "OeecT8bp3pwQzjnnnHPOOeecc84555wgNGQVAAAEAEAQho1h3CkI0udoIEYRYhoy6UH36DAJGoOcQurR6GiklDoIJZVxUkon"
    "CA1ZBQAAAgBACCGFFFJIIYUUUkghhRRiiCGGGHLKKaeggkoqqaiijDLLLLPMMssss8w67KyzDjsMMcQQQyutxFJTbTXWWGvu"
    "Oeeag7RWWmuttVJKKaWUUgpCQ1YBACAAAARCBhlkkFFIIYUUYogpp5xyCiqogNCQVQAAIACAAAAAAE/yHNERHdERHdERHdER"
    "HdHxHM8RJVESJVESLdMyNdNTRVV1ZdeWdVm3fVvYhV33fd33fd34dWFYlmVZlmVZlmVZlmVZlmVZliA0ZBUAAAIAACCEEEJI"
    "IYUUUkgpxhhzzDnoJJQQCA1ZBQAAAgAIAAAAcBRHcRzJkRxJsiRL0iTN0ixP8zRPEz1RFEXTNFXRFV1RN21RNmXTNV1TNl1V"
    "Vm1Xlm1btnXbl2Xb933f933f933f933f931dB0JDVgEAEgAAOpIjKZIiKZLjOI4kSUBoyCoAQAYAQAAAiuIojuM4kiRJkiVp"
    "kmd5lqiZmumZniqqQGjIKgAAEABAAAAAAAAAiqZ4iql4iqh4juiIkmiZlqipmivKpuy6ruu6ruu6ruu6ruu6ruu6ruu6ruu6"
    "ruu6ruu6ruu6ruu6rguEhqwCACQAAHQkR3IkR1IkRVIkR3KA0JBVAIAMAIAAABzDMSRFcizL0jRP8zRPEz3REz3TU0VXdIHQ"
    "kFUAACAAgAAAAAAAAAzJsBTL0RxNEiXVUi1VUy3VUkXVU1VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVU3TNE0T"
    "CA1ZCQAAAQDQWnPMrZeOQeisl8gopKDXTjnmpNfMKIKc5xAxY5jHUjFDDMaWQYSUBUJDVgQAUQAAgDHIMcQccs5J6iRFzjkq"
    "HaXGOUepo9RRSrGmWjtKpbZUa+Oco9RRyiilWkurHaVUa6qxAACAAAcAgAALodCQFQFAFAAAgQxSCimFlGLOKeeQUso55hxi"
    "ijmnnGPOOSidlMo5J52TEimlnGPOKeeclM5J5pyT0kkoAAAgwAEAIMBCKDRkRQAQJwDgcBxNkzRNFCVNE0VPFF3XE0XVlTTN"
    "NDVRVFVNFE3VVFVZFk1VliVNM01NFFVTE0VVFVVTlk1VtWXPNG3ZVFXdFlXVtmVb9n1XlnXdM03ZFlXVtk1VtXVXlnVdtm3d"
    "lzTNNDVRVFVNFFXXVFXbNlXVtjVRdF1RVWVZVFVZdl1Z11VX1n1NFFXVU03ZFVVVllXZ1WVVlnVfdFXdVl3Z11VZ1n3b1oVf"
    "1n3CqKq6bsqurquyrPuyLvu67euUSdNMUxNFVdVEUVVNV7VtU3VtWxNF1xVV1ZZFU3VlVZZ9X3Vl2ddE0XVFVZVlUVVlWZVl"
    "XXdlV7dFVdVtVXZ933RdXZd1XVhmW/eF03V1XZVl31dlWfdlXcfWdd/3TNO2TdfVddNVdd/WdeWZbdv4RVXVdVWWhV+VZd/X"
    "heF5bt0XnlFVdd2UXV9XZVkXbl832r5uPK9tY9s+sq8jDEe+sCxd2za6vk2Ydd3oG0PhN4Y007Rt01V13XRdX5d13WjrulBU"
    "VV1XZdn3VVf2fVv3heH2fd8YVdf3VVkWhtWWnWH3faXuC5VVtoXf1nXnmG1dWH7j6Py+MnR1W2jrurHMvq48u3F0hj4CAAAG"
    "HAAAAkwoA4WGrAgA4gQAGIScQ0xBiBSDEEJIKYSQUsQYhMw5KRlzUkIpqYVSUosYg5A5JiVzTkoooaVQSkuhhNZCKbGFUlps"
    "rdWaWos1hNJaKKW1UEqLqaUaW2s1RoxByJyTkjknpZTSWiiltcw5Kp2DlDoIKaWUWiwpxVg5JyWDjkoHIaWSSkwlpRhDKrGV"
    "lGIsKcXYWmy5xZhzKKXFkkpsJaVYW0w5thhzjhiDkDknJXNOSiiltVJSa5VzUjoIKWUOSiopxVhKSjFzTkoHIaUOQkolpRhT"
    "SrGFUmIrKdVYSmqxxZhzSzHWUFKLJaUYS0oxthhzbrHl1kFoLaQSYyglxhZjrq21GkMpsZWUYiwp1RZjrb3FmHMoJcaSSo0l"
    "pVhbjbnGGHNOseWaWqy5xdhrbbn1mnPQqbVaU0y5thhzjrkFWXPuvYPQWiilxVBKjK21WluMOYdSYisp1VhKirXFmHNrsfZQ"
    "SowlpVhLSjW2GGuONfaaWqu1xZhrarHmmnPvMebYU2s1txhrTrHlWnPuvebWYwEAAAMOAAABJpSBQkNWAgBRAAAEIUoxBqFB"
    "iDHnpDQIMeaclIox5yCkUjHmHIRSMucglJJS5hyEUlIKpaSSUmuhlFJSaq0AAIACBwCAABs0JRYHKDRkJQCQCgBgcBzL8jxR"
    "NFXZdizJ80TRNFXVth3L8jxRNE1VtW3L80TRNFXVdXXd8jxRNFVVdV1d90RRNVXVdWVZ9z1RNFVVdV1Z9n3TVFXVdWVZtoVf"
    "NFVXdV1ZlmXfWF3VdWVZtnVbGFbVdV1Zlm1bN4Zb13Xd94VhOTq3buu67/vC8TvHAADwBAcAoAIbVkc4KRoLLDRkJQCQAQBA"
    "GIOQQUghgxBSSCGlEFJKCQAAGHAAAAgwoQwUGrISAIgCAAAIkVJKKY2UUkoppZFSSimllBJCCCGEEEIIIYQQQgghhBBCCCGE"
    "EEIIIYQQQgghhBBCCAUA+E84APg/2KApsThAoSErAYBwAADAGKWYcgw6CSk1jDkGoZSUUmqtYYwxCKWk1FpLlXMQSkmptdhi"
    "rJyDUFJKrcUaYwchpdZarLHWmjsIKaUWa6w52BxKaS3GWHPOvfeQUmsx1lpz772X1mKsNefcgxDCtBRjrrn24HvvKbZaa809"
    "+CCEULHVWnPwQQghhIsx99yD8D0IIVyMOecehPDBB2EAAHeDAwBEgo0zrCSdFY4GFxqyEgAICQAgEGKKMeecgxBCCJFSjDnn"
    "HIQQQiglUoox55yDDkIIJWSMOecchBBCKKWUjDHnnIMQQgmllJI55xyEEEIopZRSMueggxBCCaWUUkrnHIQQQgillFJK6aCD"
    "EEIJpZRSSikhhBBCCaWUUkopJYQQQgmllFJKKaWEEEoopZRSSimllBBCKaWUUkoppZQSQiillFJKKaWUkkIppZRSSimllFJS"
    "KKWUUkoppZRSSgmllFJKKaWUlFJJBQAAHDgAAAQYQScZVRZhowkXHoBCQ1YCAEAAABTEVlOJnUHMMWepIQgxqKlCSimGMUPK"
    "IKYpUwohhSFziiECocVWS8UAAAAQBAAICAkAMEBQMAMADA4QPgdBJ0BwtAEACEJkhkg0LASHB5UAETEVACQmKOQCQIXFRdrF"
    "BXQZ4IIu7joQQhCCEMTiAApIwMEJNzzxhifc4ASdolIHAQAAAABgAAAPAADHBRAR0RxGhsYGR4fHB0hIAAAAAAC4AMAHAMAh"
    "AkRENIeRobHB0eHxARISAAAAAAAAAAAABAQEAAAAAAACAAAABARPZ2dTAASAFgAAAAAAAB5PvnoCAAAAbcoG3ggcOikoKihe"
    "S1zjKxlVddg/DCCAi9q2W2xvvvlmdBiGYRiGYQKaCL4MXATQ/d6tE77JqCmSCgAAAAAAAAAAAAAInrcMQDjszWfe/PqzH58p"
    "XtDVbfYfvuXNk8eNagAAvgi+KBcBuJ7jNeGb1YeacToAAACAAQAAAAAAAERPAQAhhTxLj68AACa+CL6oFwG4Pcdjwjfbh5ri"
    "dAAAAAAAAAAAAAAA2m8CAKSQ4oj5GyQAvgi+qBcBpD3HY8I3ij7UFKcDAAAAGAAAAAAAAMD+DADY4AT7l7kJAKAD3gi+qhcB"
    "pD3HY8I3inWoGacDAAAAAAAAAAAAADC/AwDB1mz4ouEKAD7Y3Q5eqfvefb7hDTVFUgEAJIEBAAAAAADgNuCm1bjnSLta8Nmo"
    "Oo0xkRu83DWCdlk5VdbKkWE3VNLjkmE3kB5XJBwfKunb12s4zTBd+vYVCbuhkr59RcJuIIXSlwl+tvzf2SL4ae0eN+o3QtQU"
    "N5Lt8rtibG9HUAIAAAAAAADsb/2O4GvUu9zd/nvjwYnN2Gik0aoDvd4quJHuunJoioapxDCUMAw+VAA=";

}  // namespace

TEST_CASE("ogg vorbis decoder probes and decodes utf8 paths without ffmpeg") {
    const auto bytes = decode_base64(kTinyOggBase64);
    REQUIRE_FALSE(bytes.empty());

    const std::filesystem::path temp_dir =
        std::filesystem::temp_directory_path() / "tenriff_ogg_decoder_tests";
    std::filesystem::create_directories(temp_dir);
    const std::filesystem::path ogg_path = temp_dir / std::filesystem::u8path(u8"오디오.ogg");

    {
        std::ofstream out(ogg_path, std::ios::binary);
        REQUIRE(out.good());
        out.write(reinterpret_cast<const char*>(bytes.data()),
                  static_cast<std::streamsize>(bytes.size()));
    }

    std::string probe_error;
    const auto probed_rate =
        tenriff::audio::probe_ogg_vorbis_sample_rate(ogg_path.u8string(), &probe_error);
    if (!probed_rate.has_value()) {
        throw doctest::TestFailure(std::string("probe_ogg_vorbis_sample_rate failed: ") + probe_error);
    }
    CHECK(*probed_rate == 48000);

    std::vector<float> decoded;
    int sample_rate = 0;
    std::string decode_error;
    if (!tenriff::audio::decode_ogg_vorbis_stereo(ogg_path.u8string(), &sample_rate, decoded, &decode_error)) {
        throw doctest::TestFailure(std::string("decode_ogg_vorbis_stereo failed: ") + decode_error);
    }
    CHECK(sample_rate == 48000);
    CHECK_FALSE(decoded.empty());
    CHECK((decoded.size() % 2u) == 0u);
    CHECK(decoded.size() >= 4800u);
    const auto peak_it = std::max_element(decoded.begin(), decoded.end(), [](float lhs, float rhs) {
        return std::abs(lhs) < std::abs(rhs);
    });
    REQUIRE(peak_it != decoded.end());
    CHECK(std::abs(*peak_it) > 0.01f);

    std::error_code ec;
    std::filesystem::remove(ogg_path, ec);
    std::filesystem::remove(temp_dir, ec);
}

namespace {
struct OggTemp {
    std::filesystem::path path=std::filesystem::temp_directory_path()/
        ("tenriff-ogg-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    OggTemp() { std::filesystem::create_directories(path); }
    ~OggTemp() { std::error_code ec; std::filesystem::remove_all(path,ec); }
};
void write_ogg(const std::filesystem::path& p,const ogg_test::Bytes& b) {
    std::ofstream f(p,std::ios::binary); f.write(reinterpret_cast<const char*>(b.data()),b.size()); REQUIRE(f.good());
}
}

TEST_CASE("ogg vorbis malformed headers reject in both public entry points") {
    OggTemp temp; auto path=temp.path/std::filesystem::u8path(u8"잘린헤더.ogg");
    const auto packets=ogg_test::packets(ogg_test::read(ogg_test::fixture("mono22050.ogg")));
    std::vector<ogg_test::Bytes> inputs;
    for(auto count:{0x20000001u,0x7fffffffu,0x80000000u,0xffffffffu})
        inputs.push_back(ogg_test::stream(packets[0],ogg_test::comment(count),packets[2]));
    for(auto n:{0x7ffffff8u,0x7fffffffu,0xffffffffu})
        inputs.push_back(ogg_test::stream(packets[0],ogg_test::comment(1,n),packets[2]));
    for(std::size_t i=0;i<packets[1].size();++i)
        inputs.push_back(ogg_test::stream(packets[0],ogg_test::Bytes(packets[1].begin(),packets[1].begin()+i),packets[2]));
    DWORD before=0,after=0; REQUIRE(GetProcessHandleCount(GetCurrentProcess(),&before));
    for(int repeat=0;repeat<3;++repeat) for(const auto& b:inputs) {
        write_ogg(path,b); std::string error; std::vector<float> out{1}; int rate=1;
        CHECK_FALSE(tenriff::audio::probe_ogg_vorbis_sample_rate(path.u8string(),&error).has_value());
        CHECK_FALSE(error.empty());
        CHECK_FALSE(tenriff::audio::decode_ogg_vorbis_stereo(path.u8string(),&rate,out,&error,1));
        CHECK(out.empty()); CHECK(rate==0);
    }
    REQUIRE(GetProcessHandleCount(GetCurrentProcess(),&after)); CHECK(after<=before+2);
}

TEST_CASE("ogg vorbis normal rates stereo long audio and bounded previews") {
    OggTemp temp;
    struct Control { const char* name; int rate; bool mono; };
    for(const auto c:{Control{"mono22050.ogg",22050,true},Control{"stereo44100.ogg",44100,false},
                     Control{"stereo96000.ogg",96000,false},Control{"long48000.ogg",48000,true}}) {
        const auto path=temp.path/std::filesystem::u8path(u8"정상음원.ogg");
        write_ogg(path,ogg_test::read(ogg_test::fixture(c.name)));
        auto probe=tenriff::audio::probe_ogg_vorbis_sample_rate(path.u8string());
        REQUIRE(probe.has_value()); CHECK(*probe==c.rate);
        std::vector<float> full,limited; int rate=0; std::string error;
        REQUIRE(tenriff::audio::decode_ogg_vorbis_stereo(path.u8string(),&rate,full,&error));
        CHECK(rate==c.rate); REQUIRE(full.size()>128);
        if(c.rate==48000) CHECK(full.size()>=65u*48000u*2u);
        for(float sample:full) CHECK(std::isfinite(sample));
        if(c.mono) for(std::size_t i=0;i<full.size();i+=2) CHECK(full[i]==full[i+1]);
        for(std::size_t limit:{1u,64u,4096u}) {
            REQUIRE(tenriff::audio::decode_ogg_vorbis_stereo(path.u8string(),&rate,limited,&error,limit));
            CHECK(limited.size()==std::min(full.size(),limit*2));
            CHECK(std::equal(limited.begin(),limited.end(),full.begin()));
        }
        auto chart=temp.path/"preview.bms";
        { std::ofstream f(chart); f<<"#PLAYER 1\n#BPM 120\n#WAV01 "<<path.filename().u8string()<<"\n#00011:01\n"; }
        std::vector<float> preview; std::string source;
        REQUIRE(tenriff::app::build_song_preview_audio(chart.u8string(),path.u8string(),48000,1,preview,source,&error));
        // The existing preview contract clamps requests to at least five seconds.
        CHECK_FALSE(preview.empty()); CHECK(preview.size()<=5u*96000u);
        tenriff::app::BmsEditorPreviewRequest request;
        request.chart_text="#PLAYER 1\n#BPM 120\n#WAV01 "+path.filename().u8string()+"\n#00011:01\n";
        request.source_chart_path=chart.u8string(); request.keysound_id="01"; request.keysound_only=true;
        request.max_duration_seconds=1;
        auto result=tenriff::app::build_bms_editor_preview(request);
        REQUIRE(result.success()); CHECK_FALSE(result.stereo_samples->empty());
    }
}
TEST_CASE("ogg vorbis retains Media Foundation fallback for non Vorbis input") {
    auto path=ogg_test::fixture("opus48000.ogg");
    CHECK_FALSE(tenriff::audio::probe_ogg_vorbis_sample_rate(path.u8string()).has_value());
    std::vector<float> out; std::string error;
    const bool opus=tenriff::app::decode_audio_file_stereo_resampled(path.u8string(),48000,out,&error,4800);
    std::cout<<"non-Vorbis Ogg Opus MF available="<<opus<<" "<<error<<"\n";
    if(opus) { CHECK_FALSE(out.empty()); CHECK(out.size()<=9600); }
    else CHECK(error.find("MF fallback:")!=std::string::npos);
    // MF sniffs the container: a WAV named .ogg exercises successful fallback
    // even on Windows installations without an Ogg/Opus Media Foundation handler.
    OggTemp temp; path=temp.path/"fallback.ogg";
    ogg_test::Bytes b={'R','I','F','F'}; ogg_test::le32(b,36+128);
    for(auto c:std::string("WAVEfmt "))b.push_back(c);
    ogg_test::le32(b,16); b.insert(b.end(),{1,0,1,0});
    ogg_test::le32(b,48000); ogg_test::le32(b,96000); b.insert(b.end(),{2,0,16,0,'d','a','t','a'});
    ogg_test::le32(b,128); for(int i=0;i<64;++i)b.insert(b.end(),{0,64}); write_ogg(path,b);
    CHECK_FALSE(tenriff::audio::probe_ogg_vorbis_sample_rate(path.u8string()).has_value());
    REQUIRE(tenriff::app::decode_audio_file_stereo_resampled(path.u8string(),48000,out,&error));
    CHECK(out.size()==128); CHECK(out.front()>0.1f);
}
#endif
