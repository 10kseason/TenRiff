#pragma once
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>

namespace ogg_test {
using Bytes = std::vector<unsigned char>;
inline Bytes read(const std::filesystem::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) throw std::runtime_error("OGG fixture missing");
    return Bytes(std::istreambuf_iterator<char>(f), {});
}
inline void le32(Bytes& b, std::uint32_t n) {
    for (int i = 0; i < 4; ++i) b.push_back(static_cast<unsigned char>(n >> (8*i)));
}
inline std::vector<Bytes> packets(const Bytes& b) {
    std::vector<Bytes> result;
    Bytes packet;
    for (std::size_t p = 0; p < b.size();) {
        if (p + 27 > b.size()) throw std::runtime_error("truncated fixture page");
        const auto segments = b[p+26];
        std::size_t data = p+27+segments;
        for (int i=0; i<segments; ++i) {
            const auto n=b.at(p+27+i);
            if (data+n > b.size()) throw std::runtime_error("truncated fixture packet");
            packet.insert(packet.end(), b.begin()+data, b.begin()+data+n);
            data+=n;
            if (n<255) { result.push_back(packet); packet.clear(); }
        }
        p=data;
    }
    return result;
}
inline Bytes page(const Bytes& packet, unsigned sequence, unsigned flags=0) {
    Bytes b={'O','g','g','S',0,static_cast<unsigned char>(flags)};
    b.resize(14,0); le32(b,1); le32(b,sequence); le32(b,0);
    const auto count=packet.size()/255+1;
    if (count>255) throw std::runtime_error("fixture packet too large");
    b.push_back(static_cast<unsigned char>(count));
    for (std::size_t i=0;i<count;++i) b.push_back(static_cast<unsigned char>(i+1<count?255:packet.size()%255));
    b.insert(b.end(),packet.begin(),packet.end());
    std::uint32_t crc=0;
    for (auto c:b) { crc^=std::uint32_t(c)<<24; for(int k=0;k<8;++k) crc=(crc<<1)^((crc&0x80000000u)?0x04c11db7u:0); }
    for (int i=0;i<4;++i) b[22+i]=static_cast<unsigned char>(crc>>(8*i));
    return b;
}
inline Bytes stream(const Bytes& identification, const Bytes& comment, const Bytes& setup={}) {
    auto b=page(identification,0,2);
    auto p=page(comment,1); b.insert(b.end(),p.begin(),p.end());
    if (!setup.empty()) { p=page(setup,2); b.insert(b.end(),p.begin(),p.end()); }
    return b;
}
inline Bytes comment(std::uint32_t count, std::uint32_t length=0) {
    Bytes b={3,'v','o','r','b','i','s'};
    le32(b,0); le32(b,count);
    if(count) le32(b,length);
    // More than one zero-length entry makes an undersized pointer array observable.
    for(int i=0;i<8;++i) le32(b,0);
    b.push_back(1); return b;
}
inline std::filesystem::path fixture(const char* name) {
    return std::filesystem::u8path(TENRIFF_SOURCE_DIR)/"tests/fixtures/ogg"/name;
}
}
