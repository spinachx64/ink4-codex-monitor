#include "agy_state.h"
#include <cJSON.h>
#include <cmath>
#include <cstring>

namespace quota {
namespace {

const cJSON* field(const cJSON* obj, const char* key) {
    return cJSON_GetObjectItemCaseSensitive(obj, key);
}

bool finite(const cJSON* obj) {
    return cJSON_IsNumber(obj) && std::isfinite(obj->valuedouble);
}

bool integer(const cJSON* obj, double min, double max) {
    return finite(obj) && obj->valuedouble >= min && obj->valuedouble <= max &&
           std::floor(obj->valuedouble) == obj->valuedouble;
}

bool parseWindow(const cJSON* obj, Window& win, int64_t sentMs, int64_t nowUs) {
    if (cJSON_IsNull(obj)) return true;
    if (!cJSON_IsObject(obj)) return false;
    auto u = field(obj, "u"), r = field(obj, "r"), s = field(obj, "s");
    if (!u || !r || !cJSON_IsString(s)) return false;
    if (!cJSON_IsNull(u) && (!finite(u) || u->valuedouble < 0 || u->valuedouble > 100)) return false;
    if (!cJSON_IsNull(r) && !integer(r, 1, 99999999999.)) return false;
    if (!strcmp(s->valuestring, "api")) win.source = Source::Api;
    else if (!strcmp(s->valuestring, "estimated")) win.source = Source::Estimated;
    else if (!strcmp(s->valuestring, "stale")) win.source = Source::Stale;
    else if (!strcmp(s->valuestring, "unavailable")) win.source = Source::Unavailable;
    else return false;
    if (win.source == Source::Api && !finite(u)) return false;
    if (win.source == Source::Estimated && (!cJSON_IsNull(u) || !integer(r, 1, 99999999999.))) return false;
    win.hasUsage = finite(u) && (win.source == Source::Api || win.source == Source::Stale);
    win.used = win.hasUsage ? u->valuedouble : 0;
    win.resetSec = cJSON_IsNumber(r) ? static_cast<int64_t>(r->valuedouble) : 0;
    win.updatedUs = nowUs;
    win.clock.anchor(win.resetSec, sentMs, nowUs);
    return true;
}

bool parsePair(const cJSON* obj, Window out[2], int64_t sentMs, int64_t nowUs) {
    if (!cJSON_IsObject(obj)) return false;
    return parseWindow(field(obj, "h"), out[0], sentMs, nowUs) &&
           parseWindow(field(obj, "w"), out[1], sentMs, nowUs);
}

}  // namespace

bool AgyStore::apply(const char* line, size_t length, int64_t receivedUs, bool resetConfirmed[2]) {
    resetConfirmed[0] = resetConfirmed[1] = false;
    if (!line || !length || length >= 512 || memchr(line, '\0', length)) return false;
    const char* end = nullptr;
    cJSON* root = cJSON_ParseWithLengthOpts(line, length + 1, &end, true);
    if (!root) return false;
    Window nextGem[2], nextExt[2];
    auto v = field(root, "v"), k = field(root, "k"), q = field(root, "q"), t = field(root, "t");
    bool valid = cJSON_IsObject(root) && integer(v, 2, 2) &&
                 cJSON_IsString(k) && !strcmp(k->valuestring, "a") &&
                 integer(q, 1, 9007199254740991.) && q->valuedouble > sequence &&
                 integer(t, 1, 99999999999999.) &&
                 parsePair(field(root, "g"), nextGem, t->valuedouble, receivedUs) &&
                 parsePair(field(root, "c"), nextExt, t->valuedouble, receivedUs);
    if (valid) {
        for (int i = 0; i < 2; ++i) {
            resetConfirmed[i] = lastApiReset[i] > 0 && nextGem[i].source == Source::Api &&
                                nextGem[i].resetSec > lastApiReset[i];
            if (nextGem[i].source == Source::Stale && gemini[i].resetSec == nextGem[i].resetSec)
                nextGem[i].updatedUs = gemini[i].updatedUs;
            if (nextGem[i].source == Source::Api && nextGem[i].resetSec > 0)
                lastApiReset[i] = nextGem[i].resetSec;
            gemini[i] = nextGem[i];

            if (nextExt[i].source == Source::Stale && external[i].resetSec == nextExt[i].resetSec)
                nextExt[i].updatedUs = external[i].updatedUs;
            external[i] = nextExt[i];
        }
        sequence = q->valuedouble;
    }
    cJSON_Delete(root);
    return valid;
}

}  // namespace quota
