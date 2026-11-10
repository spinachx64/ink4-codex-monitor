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
}
