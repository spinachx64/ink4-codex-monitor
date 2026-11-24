#include "model/agy_state.h"
#include "model/quota_state.h"
#include <cassert>
#include <cstring>
int main() {
    quota::Store s;
    bool reset[2];
    auto apply = [&](const char* packet) { return s.apply(packet, strlen(packet), 1000000, reset); };
    const char* good = R"({"v":1,"q":1,"t":1000000,"h":{"u":25,"r":1100,"s":"api"},"w":null})";
    assert(apply(good));
    assert(s.windows[0].remaining() == 75);
    assert(!s.windows[1].hasUsage);
    assert(s.windows[0].clock.remaining(1000000) == 100000);
    assert(s.windows[0].status(101000000) == quota::Status::ResetPending);
    assert(!apply(good)); // replay must not renew data
    assert(!apply(R"({"v":1,"q":2,"t":1000000,"h":{"u":101,"r":1100,"s":"api"},"w":null})"));
    assert(s.windows[0].remaining() == 75); // invalid updates are atomic
    assert(apply(R"({"v":1,"q":2,"t":1000000,"h":null,"w":{"u":100,"r":2000,"s":"api"}})"));
    assert(!s.windows[0].hasUsage);
    assert(s.windows[1].remaining() == 0);

    // AGY v2 packet tests and cross-protocol isolation.
    quota::AgyStore agy;
    auto applyAgy = [&](const char* packet) { return agy.apply(packet, strlen(packet), 1000000, reset); };
    assert(!applyAgy(good)); // v1 Codex packet rejected by AgyStore
    const char* agyGood =
        R"({"v":2,"k":"a","q":10,"t":1000000,"g":{"h":{"u":4,"r":1100,"s":"api"},"w":{"u":1,"r":2000,"s":"api"}},"c":{"h":{"u":36,"r":null,"s":"api"},"w":{"u":18,"r":null,"s":"api"}}})";
    assert(!apply(agyGood)); // v2 AGY packet rejected by Codex Store
    assert(applyAgy(agyGood));
    assert(agy.gemini[0].remaining() == 96);
    assert(agy.gemini[1].remaining() == 99);
    assert(agy.external[0].remaining() == 64);
    assert(agy.external[1].remaining() == 82);
    assert(!agy.external[0].clock.valid);
    assert(agy.gemini[0].clock.remaining(1000000) == 100000);
    assert(!applyAgy(agyGood)); // replay rejected
    assert(!applyAgy(R"({"v":2,"k":"a","q":11,"t":1000000,"g":{"h":{"u":101,"r":1100,"s":"api"},"w":null},"c":{"h":null,"w":null}})"));
    assert(agy.gemini[0].remaining() == 96); // atomic rejection
}
