#pragma once
// Wrapper minimo de libcurl para las integraciones (Google, iCloud) y utilidades
// de red. El correo SMTP tiene su propio modulo (Email).

#include <string>
#include <vector>

namespace agenda {

struct HttpResponse {
    long        status = 0;
    std::string body;
    std::string error;      // error de transporte (no HTTP)
    bool ok() const { return status >= 200 && status < 300; }
};

struct HttpRequest {
    std::string method = "GET";                 // GET/POST/PUT/DELETE/PROPFIND/REPORT
    std::string url;
    std::string body;
    std::vector<std::string> headers;           // "Clave: Valor"
    std::string basicUser;                      // auth basica opcional
    std::string basicPass;
    std::string depth;                          // cabecera Depth para CalDAV (opcional)
    long timeoutSeg = 30;
};

// Inicializa libcurl una sola vez (seguro llamar varias veces).
void httpInit();

HttpResponse httpRequest(const HttpRequest& req);

// Codifica un valor para usarlo en application/x-www-form-urlencoded.
std::string urlEncode(const std::string& s);

} // namespace agenda
