#pragma once
// Envio de correo por SMTP (equivale a nodemailer en main.js). Usa libcurl.

#include "app/Model.h"
#include <string>

namespace agenda {

// Devuelven cadena vacia si todo fue bien, o un mensaje de error en español.
std::string emailEnviar(const EmailCfg& cfg, const std::string& asunto, const std::string& cuerpo);
std::string emailProbar(const EmailCfg& cfg);   // envia un correo de prueba a uno mismo

} // namespace agenda
