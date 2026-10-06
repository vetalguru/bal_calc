// JNI entry points of org.vetalguru.balcalc.core.Native (Android and
// desktop): a handle to a bridge::Api and its one call.
#include <jni.h>

#include <string>

#include <ballistics/bridge/api.h>

namespace {

using ballistics::bridge::Api;

Api* FromHandle(jlong handle) { return reinterpret_cast<Api*>(handle); }

// Java strings are UTF-16; GetStringUTFChars gives "modified UTF-8", which
// differs for NUL and characters outside the BMP, so convert explicitly.
std::string ToUtf8(JNIEnv* env, jstring s) {
    if (s == nullptr) {
        return {};
    }
    const jsize len = env->GetStringLength(s);
    const jchar* chars = env->GetStringChars(s, nullptr);
    std::string out;
    out.reserve(static_cast<std::size_t>(len));
    for (jsize i = 0; i < len; ++i) {
        char32_t c = chars[i];
        if (c >= 0xD800 && c <= 0xDBFF && i + 1 < len && chars[i + 1] >= 0xDC00 &&
            chars[i + 1] <= 0xDFFF) {
            c = 0x10000 + ((c - 0xD800) << 10) + (chars[i + 1] - 0xDC00);
            ++i;
        }
        if (c < 0x80) {
            out += static_cast<char>(c);
        } else if (c < 0x800) {
            out += static_cast<char>(0xC0 | (c >> 6));
            out += static_cast<char>(0x80 | (c & 0x3F));
        } else if (c < 0x10000) {
            out += static_cast<char>(0xE0 | (c >> 12));
            out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (c & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (c >> 18));
            out += static_cast<char>(0x80 | ((c >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (c & 0x3F));
        }
    }
    env->ReleaseStringChars(s, chars);
    return out;
}

jstring FromUtf8(JNIEnv* env, const std::string& s) {
    std::u16string out;
    out.reserve(s.size());
    for (std::size_t i = 0; i < s.size();) {
        const auto byte = [&s, i](std::size_t k) {
            return static_cast<char32_t>(static_cast<unsigned char>(s[i + k]));
        };
        const auto cont = [&byte](std::size_t k) { return byte(k) & 0x3Fu; };
        const char32_t b = byte(0);
        char32_t c = 0xFFFD;
        std::size_t n = 1;
        if (b < 0x80) {
            c = b;
        } else if ((b >> 5) == 0x6 && i + 1 < s.size()) {
            c = ((b & 0x1Fu) << 6) | cont(1);
            n = 2;
        } else if ((b >> 4) == 0xE && i + 2 < s.size()) {
            c = ((b & 0x0Fu) << 12) | (cont(1) << 6) | cont(2);
            n = 3;
        } else if ((b >> 3) == 0x1E && i + 3 < s.size()) {
            c = ((b & 0x07u) << 18) | (cont(1) << 12) | (cont(2) << 6) | cont(3);
            n = 4;
        }
        i += n;
        if (c >= 0x10000) {
            c -= 0x10000;
            out += static_cast<char16_t>(0xD800 + (c >> 10));
            out += static_cast<char16_t>(0xDC00 + (c & 0x3FF));
        } else {
            out += static_cast<char16_t>(c);
        }
    }
    return env->NewString(reinterpret_cast<const jchar*>(out.data()), static_cast<jsize>(out.size()));
}

} // namespace

extern "C" {

JNIEXPORT jlong JNICALL Java_org_vetalguru_balcalc_core_Native_create(JNIEnv*, jclass) {
    return reinterpret_cast<jlong>(new Api());
}

JNIEXPORT void JNICALL Java_org_vetalguru_balcalc_core_Native_destroy(JNIEnv*, jclass,
                                                                      jlong handle) {
    delete FromHandle(handle);
}

JNIEXPORT jstring JNICALL Java_org_vetalguru_balcalc_core_Native_call(JNIEnv* env, jclass,
                                                                      jlong handle,
                                                                      jstring method,
                                                                      jstring args) {
    return FromUtf8(env, FromHandle(handle)->Call(ToUtf8(env, method), ToUtf8(env, args)));
}

} // extern "C"
