#include "net/Http.h"

#include <curl/curl.h>
#include <mutex>

namespace agenda {

static std::once_flag g_initFlag;

void httpInit() {
    std::call_once(g_initFlag, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });
}

static size_t escribir(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* out = static_cast<std::string*>(userdata);
    out->append(ptr, size * nmemb);
    return size * nmemb;
}

std::string urlEncode(const std::string& s) {
    httpInit();
    CURL* c = curl_easy_init();
    if (!c) return s;
    char* e = curl_easy_escape(c, s.c_str(), static_cast<int>(s.size()));
    std::string out = e ? e : s;
    if (e) curl_free(e);
    curl_easy_cleanup(c);
    return out;
}

HttpResponse httpRequest(const HttpRequest& req) {
    httpInit();
    HttpResponse resp;

    CURL* c = curl_easy_init();
    if (!c) { resp.error = "No se pudo inicializar libcurl"; return resp; }

    curl_easy_setopt(c, CURLOPT_URL, req.url.c_str());
    curl_easy_setopt(c, CURLOPT_CUSTOMREQUEST, req.method.c_str());
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, escribir);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &resp.body);
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, req.timeoutSeg);
    curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(c, CURLOPT_USERAGENT, "AgendaPersonal/1.0");
    curl_easy_setopt(c, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(c, CURLOPT_SSL_VERIFYHOST, 2L);

    if (!req.body.empty() || req.method == "POST" || req.method == "PUT") {
        curl_easy_setopt(c, CURLOPT_POSTFIELDS, req.body.c_str());
        curl_easy_setopt(c, CURLOPT_POSTFIELDSIZE, static_cast<long>(req.body.size()));
    }

    if (!req.basicUser.empty()) {
        curl_easy_setopt(c, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
        curl_easy_setopt(c, CURLOPT_USERNAME, req.basicUser.c_str());
        curl_easy_setopt(c, CURLOPT_PASSWORD, req.basicPass.c_str());
    }

    struct curl_slist* headers = nullptr;
    for (const auto& h : req.headers) headers = curl_slist_append(headers, h.c_str());
    if (!req.depth.empty()) headers = curl_slist_append(headers, ("Depth: " + req.depth).c_str());
    if (headers) curl_easy_setopt(c, CURLOPT_HTTPHEADER, headers);

    CURLcode rc = curl_easy_perform(c);
    if (rc != CURLE_OK) {
        resp.error = curl_easy_strerror(rc);
    } else {
        curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &resp.status);
    }

    if (headers) curl_slist_free_all(headers);
    curl_easy_cleanup(c);
    return resp;
}

} // namespace agenda
