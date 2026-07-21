#pragma once
// Modelo de datos de la Agenda Personal (equivalente a los objetos que maneja
// el store.js / app.js del proyecto Electron original), con (de)serializacion JSON.

#include <string>
#include <vector>
#include <cstdint>
#include <nlohmann/json.hpp>

namespace agenda {

// -------------------------------- Tarea -------------------------------------
struct Tarea {
    std::string id;
    std::string titulo;
    std::string fechaLimite;              // "YYYY-MM-DD"
    std::string notas;
    std::vector<int> avisosPrevios;       // dias antes del ultimo dia (ej. 1,3,7)
    std::vector<std::string> horarios;    // "HH:MM"
    bool completada = false;
    bool eliminada  = false;
    int  orden      = 0;                   // (obsoleto) orden manual en "Tareas"; ya no se usa
    int  ordenHist  = -1;                  // (obsoleto) orden manual en "Historial"; ya no se usa
    float posX      = -1.0f;               // posicion libre en pantalla (-1 = sin ubicar -> auto-layout)
    float posY      = -1.0f;
    std::string creadaEn;                 // ISO 8601
    std::string completadaEn;
    std::string eliminadaEn;
    std::string googleEventId;            // id del evento en Google Calendar (si aplica)
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
    Tarea, id, titulo, fechaLimite, notas, avisosPrevios, horarios,
    completada, eliminada, orden, ordenHist, posX, posY,
    creadaEn, completadaEn, eliminadaEn, googleEventId)

// ------------------------------ Configuracion -------------------------------
struct EmailCfg {
    std::string direccion;
    std::string appPassword;
    std::string smtpHost = "smtp.gmail.com";
    int         smtpPort = 465;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
    EmailCfg, direccion, appPassword, smtpHost, smtpPort)

// Tokens OAuth de Google (se guardan tal cual para poder refrescar el acceso).
struct GoogleTokens {
    std::string accessToken;
    std::string refreshToken;
    int64_t     expiryEpochMs = 0;        // epoch en ms cuando vence el access token
    bool        valido        = false;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
    GoogleTokens, accessToken, refreshToken, expiryEpochMs, valido)

struct GoogleCfg {
    std::string  clientId;
    std::string  clientSecret;
    GoogleTokens tokens;
    bool         activo = false;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
    GoogleCfg, clientId, clientSecret, tokens, activo)

struct IcloudCfg {
    std::string appleId;
    std::string appPassword;
    bool        activo = false;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
    IcloudCfg, appleId, appPassword, activo)

struct FondoCfg {
    std::string tipo  = "degradado";      // degradado|lavanda|tulipanes|color|imagen
    std::string valor;                    // color hex (#rrggbb) o ruta de imagen
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(FondoCfg, tipo, valor)

struct NotifCfg {
    bool ventana = true;
    bool correo  = true;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(NotifCfg, ventana, correo)

struct Config {
    std::string tema          = "sistema";     // claro|oscuro|sistema
    std::string colorPrograma = "normal";      // normal|verde|coral|naranja|turquesa|lila
    std::string posicionPanel = "izquierda";   // arriba|abajo|izquierda|derecha
    FondoCfg    fondo;
    EmailCfg    email;
    bool        iniciarConWindows = false;
    GoogleCfg   googleCalendar;
    IcloudCfg   icloudReminders;
    NotifCfg    notificaciones;
    int         correccionHorariaMin = 0;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
    Config, tema, colorPrograma, posicionPanel, fondo, email, iniciarConWindows,
    googleCalendar, icloudReminders, notificaciones, correccionHorariaMin)

} // namespace agenda
