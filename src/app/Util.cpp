#include "app/Util.h"

#include <windows.h>
#include <chrono>
#include <random>
#include <cstdio>
#include <ctime>

namespace agenda {

std::string generarId() {
    // La semilla combina el reloj con el id de hilo (evita colisiones entre hilos
    // sin referenciar la propia variable durante su inicializacion, que MSVC rechaza).
    static thread_local std::mt19937_64 rng([] {
        uint64_t seed = static_cast<uint64_t>(
            std::chrono::high_resolution_clock::now().time_since_epoch().count());
        seed ^= static_cast<uint64_t>(GetCurrentThreadId()) << 32;
        return seed;
    }());
    uint64_t a = rng();
    uint64_t b = rng();
    char buf[40];
    std::snprintf(buf, sizeof(buf), "t-%016llx%016llx",
                  static_cast<unsigned long long>(a),
                  static_cast<unsigned long long>(b));
    return std::string(buf);
}

int64_t nowEpochMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

std::string nowIso() {
    std::time_t t = std::time(nullptr);
    std::tm tmUtc{};
#if defined(_WIN32)
    gmtime_s(&tmUtc, &t);
#else
    gmtime_r(&t, &tmUtc);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tmUtc);
    return std::string(buf);
}

std::string hoyLocalYmd() {
    std::time_t t = std::time(nullptr);
    std::tm tmLocal{};
#if defined(_WIN32)
    localtime_s(&tmLocal, &t);
#else
    localtime_r(&t, &tmLocal);
#endif
    char buf[16];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d", &tmLocal);
    return std::string(buf);
}

std::wstring widen(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring out(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), out.data(), n);
    return out;
}

std::string narrow(const std::wstring& s) {
    if (s.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0, nullptr, nullptr);
    std::string out(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), out.data(), n, nullptr, nullptr);
    return out;
}

} // namespace agenda
