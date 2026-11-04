// Isolated implementation: allocator fault injection never changes the product allocator.
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <climits>
#include <cassert>
#include <unordered_set>
#include <iostream>
#include "../support/OggTestFixture.h"

namespace {
std::unordered_set<void*> live;
std::size_t calls=0, fail_at=0, largest=0;
bool bad_free=false;
void* checked_malloc(std::size_t n) {
    ++calls; if(n>largest) largest=n;
    if(calls==fail_at || n>4*1024*1024) return nullptr;
    void* p=std::malloc(n); if(p) live.insert(p); return p;
}
void checked_free(void* p) {
    if(!p) return;
    if(live.erase(p)!=1) { bad_free=true; return; }
    std::free(p);
}
}
#define malloc checked_malloc
#define free checked_free
#include "../../external/stb_vorbis.c"
#undef malloc
#undef free
#undef CHECK
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

namespace {
void reset(std::size_t failure=0) {
    REQUIRE(live.empty()); REQUIRE_FALSE(bad_free);
    calls=0; fail_at=failure; largest=0;
}
bool open(const ogg_test::Bytes& b, int* error=nullptr) {
    auto* v=stb_vorbis_open_memory(b.data(),static_cast<int>(b.size()),error,nullptr);
    const bool ok=v!=nullptr; stb_vorbis_close(v);
    CHECK(live.empty()); CHECK_FALSE(bad_free); return ok;
}
}

TEST_CASE("ogg security comment sizes reach the target header and reject safely") {
    const auto packets=ogg_test::packets(ogg_test::read(ogg_test::fixture("mono22050.ogg")));
    REQUIRE(packets.size()>3);
    // Direct parser-state evidence: identification accepted, vendor read, then count rejected.
    for(auto count:{0x20000001u,0x7fffffffu,0x80000000u,0xffffffffu}) {
        reset(); auto b=ogg_test::stream(packets[0],ogg_test::comment(count),packets[2]);
        vorb v; vorbis_init(&v,nullptr); v.stream=v.stream_start=b.data(); v.stream_end=b.data()+b.size();
        v.stream_len=static_cast<unsigned>(b.size());
        CHECK_FALSE(start_decoder(&v));
        CHECK(v.channels==1); CHECK(v.sample_rate==22050); CHECK(v.vendor!=nullptr);
        CHECK(static_cast<unsigned>(v.comment_list_length)==count);
        CHECK(v.error==VORBIS_invalid_setup); vorbis_deinit(&v);
        CHECK(live.empty()); CHECK_FALSE(bad_free); CHECK(largest<1024);
    }
    for(auto len:{0x7ffffff8u,0x7fffffffu,0x80000000u,0xffffffffu}) {
        reset(); int error=0;
        CHECK_FALSE(open(ogg_test::stream(packets[0],ogg_test::comment(1,len),packets[2]),&error));
        CHECK(error==VORBIS_invalid_setup || error==VORBIS_outofmem);
        auto c=ogg_test::comment(0); for(int i=0;i<4;++i)c[7+i]=static_cast<unsigned char>(len>>(8*i));
        reset(); CHECK_FALSE(open(ogg_test::stream(packets[0],c,packets[2])));
    }
}

TEST_CASE("ogg security truncated comment and setup clean all allocations") {
    const auto p=ogg_test::packets(ogg_test::read(ogg_test::fixture("mono22050.ogg")));
    reset(); REQUIRE(open(ogg_test::stream(p[0],p[1],p[2])));
    for(std::size_t i=0;i<p[1].size();++i) {
        reset(); ogg_test::Bytes c(p[1].begin(),p[1].begin()+i);
        CHECK_FALSE(open(ogg_test::stream(p[0],c,p[2])));
    }
    for(std::size_t i=1;i<p[2].size();++i) {
        reset(); ogg_test::Bytes s(p[2].begin(),p[2].begin()+i);
        CHECK_FALSE(open(ogg_test::stream(p[0],p[1],s)));
    }
}

TEST_CASE("ogg security allocation failure sweep and repeated opens") {
    auto b=ogg_test::read(ogg_test::fixture("mono22050.ogg"));
    reset(); REQUIRE(open(b)); const auto total=calls;
    REQUIRE(total>20);
    for(std::size_t i=1;i<=total;++i) { reset(i); CHECK_FALSE(open(b)); }
    for(int i=0;i<128;++i) { reset(); REQUIRE(open(b)); }
    std::cout<<"allocation failure positions="<<total<<" repeat opens=128\n";
}

TEST_CASE("ogg security allocator alignment and codebook limits") {
    reset(); vorb v; vorbis_init(&v,nullptr);
    CHECK(setup_malloc(&v,INT_MAX)==nullptr); CHECK(setup_temp_malloc(&v,INT_MAX)==nullptr);
    CHECK(setup_malloc(&v,-1)==nullptr); CHECK(setup_temp_malloc(&v,-1)==nullptr);
    CHECK(calls==0);
    const auto p=ogg_test::packets(ogg_test::read(ogg_test::fixture("mono22050.ogg")));
    auto setup=p[2]; // first codebook: count byte, sync, dimensions LE16, entries LE24
    REQUIRE(setup[8]==0x42); REQUIRE(setup[9]==0x43); REQUIRE(setup[10]==0x56);
    setup[11]=0xff; setup[12]=0xff;
    reset(); int error=0; CHECK_FALSE(open(ogg_test::stream(p[0],p[1],setup),&error));
    CHECK(error==VORBIS_invalid_setup);
    CHECK(largest<1024*1024);
    // Custom-buffer allocation uses the same checked arithmetic and cleanup path.
    auto original=ogg_test::read(ogg_test::fixture("mono22050.ogg"));
    std::vector<char> buffer(1024*1024);
    for(int n:{64,4096,65536,1024*1024}) {
        stb_vorbis_alloc a{buffer.data(),n};
        auto* d=stb_vorbis_open_memory(original.data(),static_cast<int>(original.size()),&error,&a);
        if(n==1024*1024) REQUIRE(d!=nullptr);
        stb_vorbis_close(d);
    }
}
