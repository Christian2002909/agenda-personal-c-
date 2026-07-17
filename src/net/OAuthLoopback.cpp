#include "net/OAuthLoopback.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <cstring>

namespace agenda {

static std::string urlDecode(const std::string& s) {
    std::string o;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size()) {
            int v = std::stoi(s.substr(i + 1, 2), nullptr, 16);
            o += static_cast<char>(v); i += 2;
        } else if (s[i] == '+') { o += ' '; }
        else o += s[i];
    }
    return o;
}

std::string oauthEsperarCodigo(int timeoutSeg) {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return "";

    SOCKET srv = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (srv == INVALID_SOCKET) { WSACleanup(); return ""; }

    BOOL reuse = TRUE;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(53682);
    InetPtonW(AF_INET, L"127.0.0.1", &addr.sin_addr);

    if (bind(srv, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR ||
        listen(srv, 1) == SOCKET_ERROR) {
        closesocket(srv); WSACleanup(); return "";
    }

    // Espera con timeout a que llegue el redirect.
    fd_set fds; FD_ZERO(&fds); FD_SET(srv, &fds);
    timeval tv{}; tv.tv_sec = timeoutSeg; tv.tv_usec = 0;
    if (select(0, &fds, nullptr, nullptr, &tv) <= 0) {
        closesocket(srv); WSACleanup(); return "";
    }

    SOCKET cli = accept(srv, nullptr, nullptr);
    if (cli == INVALID_SOCKET) { closesocket(srv); WSACleanup(); return ""; }

    std::string peticion;
    char buf[2048];
    int n = recv(cli, buf, sizeof(buf) - 1, 0);
    if (n > 0) { buf[n] = 0; peticion = buf; }

    // Extrae ?code=XXX de la primera linea "GET /oauth2callback?code=...&... HTTP/1.1".
    std::string code;
    size_t pos = peticion.find("code=");
    if (pos != std::string::npos) {
        size_t start = pos + 5;
        size_t end = peticion.find_first_of("& \r\n", start);
        code = urlDecode(peticion.substr(start, end - start));
    }

    const char* html =
        "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=UTF-8\r\nConnection: close\r\n\r\n"
        "<html><body style='font-family:Segoe UI;text-align:center;padding-top:60px'>"
        "<h2>Autenticacion completa</h2><p>Ya puedes cerrar esta pestana y volver a Agenda Personal.</p>"
        "</body></html>";
    send(cli, html, (int)strlen(html), 0);

    closesocket(cli);
    closesocket(srv);
    WSACleanup();
    return code;
}

} // namespace agenda
