#include "net/Email.h"
#include "net/Http.h"

#include <curl/curl.h>
#include <string>
#include <ctime>
#include <cstring>

namespace agenda {

// Quita espacios de la contraseña de aplicacion (Gmail la muestra con espacios).
static std::string sinEspacios(const std::string& s) {
    std::string o;
    for (char c : s) if (c != ' ' && c != '\t') o += c;
    return o;
}

static std::string base64(const std::string& in) {
    static const char* T = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    int val = 0, bits = -6;
    for (unsigned char c : in) {
        val = (val << 8) + c; bits += 8;
        while (bits >= 0) { out += T[(val >> bits) & 0x3F]; bits -= 6; }
    }
    if (bits > -6) out += T[((val << 8) >> (bits + 8)) & 0x3F];
    while (out.size() % 4) out += '=';
    return out;
}

// Codifica el asunto en RFC 2047 (para que los acentos se vean bien).
static std::string encabezadoUtf8(const std::string& s) {
    return "=?UTF-8?B?" + base64(s) + "?=";
}

static std::string fechaRfc2822() {
    std::time_t t = std::time(nullptr);
    std::tm tmv{};
#if defined(_WIN32)
    gmtime_s(&tmv, &t);
#else
    gmtime_r(&t, &tmv);
#endif
    char buf[64];
    std::strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S +0000", &tmv);
    return std::string(buf);
}

struct Payload { std::string data; size_t pos = 0; };

static size_t leerPayload(char* ptr, size_t size, size_t nmemb, void* userp) {
    auto* p = static_cast<Payload*>(userp);
    size_t max = size * nmemb;
    size_t restante = p->data.size() - p->pos;
    size_t n = restante < max ? restante : max;
    if (n > 0) { memcpy(ptr, p->data.data() + p->pos, n); p->pos += n; }
    return n;
}

static std::string enviarInterno(const EmailCfg& cfg, const std::string& to,
                                 const std::string& asunto, const std::string& cuerpo) {
    if (cfg.direccion.empty() || cfg.appPassword.empty())
        return "Falta el correo o la contraseña de aplicación en Configuración.";

    httpInit();
    CURL* c = curl_easy_init();
    if (!c) return "No se pudo inicializar el envio de correo.";

    bool implicitSsl = (cfg.smtpPort == 465);
    std::string url = (implicitSsl ? "smtps://" : "smtp://") + cfg.smtpHost + ":" + std::to_string(cfg.smtpPort);

    Payload payload;
    payload.data =
        "Date: " + fechaRfc2822() + "\r\n"
        "From: " + cfg.direccion + "\r\n"
        "To: " + to + "\r\n"
        "Subject: " + encabezadoUtf8(asunto) + "\r\n"
        "MIME-Version: 1.0\r\n"
        "Content-Type: text/plain; charset=UTF-8\r\n"
        "Content-Transfer-Encoding: 8bit\r\n"
        "\r\n" + cuerpo + "\r\n";

    struct curl_slist* rcpt = nullptr;
    rcpt = curl_slist_append(rcpt, ("<" + to + ">").c_str());

    curl_easy_setopt(c, CURLOPT_URL, url.c_str());
    curl_easy_setopt(c, CURLOPT_USERNAME, cfg.direccion.c_str());
    curl_easy_setopt(c, CURLOPT_PASSWORD, sinEspacios(cfg.appPassword).c_str());
    if (implicitSsl) curl_easy_setopt(c, CURLOPT_USE_SSL, (long)CURLUSESSL_ALL);
    else             curl_easy_setopt(c, CURLOPT_USE_SSL, (long)CURLUSESSL_TRY);
    curl_easy_setopt(c, CURLOPT_MAIL_FROM, ("<" + cfg.direccion + ">").c_str());
    curl_easy_setopt(c, CURLOPT_MAIL_RCPT, rcpt);
    curl_easy_setopt(c, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(c, CURLOPT_READFUNCTION, leerPayload);
    curl_easy_setopt(c, CURLOPT_READDATA, &payload);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(c, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(c, CURLOPT_SSL_VERIFYHOST, 2L);

    CURLcode rc = curl_easy_perform(c);
    curl_slist_free_all(rcpt);
    std::string err = (rc == CURLE_OK) ? "" : std::string(curl_easy_strerror(rc));
    curl_easy_cleanup(c);

    if (!err.empty())
        return "No se pudo enviar el correo: " + err +
               ". Revisa el correo, la contraseña de aplicación y el servidor SMTP.";
    return "";
}

std::string emailEnviar(const EmailCfg& cfg, const std::string& asunto, const std::string& cuerpo) {
    return enviarInterno(cfg, cfg.direccion, asunto, cuerpo);
}

std::string emailProbar(const EmailCfg& cfg) {
    return enviarInterno(cfg, cfg.direccion, "Prueba de Agenda Personal",
        "Si recibes este correo, los avisos por email ya funcionan correctamente.");
}

} // namespace agenda
