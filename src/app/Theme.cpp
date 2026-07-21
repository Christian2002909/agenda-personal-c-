#include "app/Theme.h"

#include <windows.h>

namespace agenda {

static Col hex(unsigned int rgb, float a = 1.0f) {
    return Col{
        ((rgb >> 16) & 0xFF) / 255.0f,
        ((rgb >> 8)  & 0xFF) / 255.0f,
        ( rgb        & 0xFF) / 255.0f,
        a };
}
static Col rgba(int r, int g, int b, float a) {
    return Col{ r / 255.0f, g / 255.0f, b / 255.0f, a };
}
// mezcla "color X% opaco" como hace color-mix(in srgb, c X%, transparent).
static Col conAlfa(Col c, float a) { c.a = a; return c; }

bool sistemaPrefiereOscuro() {
    // HKCU\...\Themes\Personalize\AppsUseLightTheme (0 = oscuro, 1 = claro).
    DWORD valor = 1, tam = sizeof(valor), tipo = REG_DWORD;
    LONG r = RegGetValueW(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        L"AppsUseLightTheme", RRF_RT_REG_DWORD, &tipo, &valor, &tam);
    if (r != ERROR_SUCCESS) return false;
    return valor == 0;
}

bool temaEsOscuro(const Config& cfg) {
    if (cfg.tema == "oscuro") return true;
    if (cfg.tema == "claro")  return false;
    return sistemaPrefiereOscuro(); // "sistema"
}

static void aplicarAcento(ThemePalette& p, const std::string& color) {
    if (color == "verde")         { p.accent = hex(0x2e9e6b); p.accent2 = hex(0x43c088); }
    else if (color == "coral")    { p.accent = hex(0xef5350); p.accent2 = hex(0xff7b73); }
    else if (color == "naranja")  { p.accent = hex(0xfb8c00); p.accent2 = hex(0xffa726); }
    else if (color == "turquesa") { p.accent = hex(0x26a69a); p.accent2 = hex(0x4db6ac); }
    else if (color == "lila")     { p.accent = hex(0x9575cd); p.accent2 = hex(0x7e57c2); }
    else                          { p.accent = hex(0x4f7cff); p.accent2 = hex(0x6c63ff); } // normal
}

ThemePalette resolverPaleta(const Config& cfg) {
    ThemePalette p;
    p.isDark = temaEsOscuro(cfg);
    aplicarAcento(p, cfg.colorPrograma);

    const std::string& tipo = cfg.fondo.tipo;
    p.fondoConFoto = (tipo == "lavanda" || tipo == "tulipanes" ||
                      tipo == "imagen"  || tipo == "color");

    // Manchas radiales del degradado por defecto (base.css: accent 42%, accent2 36%).
    p.blobA = conAlfa(p.accent,  0.42f);
    p.blobB = conAlfa(p.accent2, 0.36f);

    // Comunes de badges / peligro.
    p.badge        = p.accent;
    p.badgeUrgente = hex(0xe0a02a);
    p.badgeVencida = hex(0xe0554a);
    p.peligro      = hex(0xe0554a);

    if (!p.isDark) {
        p.bg        = hex(0xeef0f7);
        p.fg        = hex(0x1d1d24);
        p.panelSolid= rgba(255,255,255,0.62f);
        p.inputBg   = rgba(255,255,255,0.88f);
        p.hoverBg   = rgba(0,0,0,0.05f);
        p.borde     = rgba(0,0,0,0.14f);
        // Vidrio "agua pura": tinte bajo para que el fondo refractado se vea
        // claro a traves del panel (antes 0.55 se veia lechoso).
        p.glassBg   = rgba(255,255,255,0.34f);
        p.glassBorde= rgba(255,255,255,0.72f);
        p.glassRim  = rgba(255,255,255,0.78f);
    } else {
        p.bg        = hex(0x12121a);
        p.fg        = hex(0xeceef5);
        p.panelSolid= rgba(255,255,255,0.10f);
        p.inputBg   = rgba(255,255,255,0.08f);
        p.hoverBg   = rgba(255,255,255,0.08f);
        p.borde     = rgba(255,255,255,0.14f);
        p.glassBg   = rgba(40,42,60,0.36f);
        p.glassBorde= rgba(255,255,255,0.16f);
        p.glassRim  = rgba(255,255,255,0.35f);
    }

    // Sobre foto o color solido: vidrio mas opaco y texto con sombra (themes.css).
    if (p.fondoConFoto) {
        if (!p.isDark) {
            p.glassBg   = rgba(255,255,255,0.42f);
            p.glassBorde= rgba(255,255,255,0.75f);
            p.glassRim  = rgba(255,255,255,0.82f);
            p.panelSolid= rgba(255,255,255,0.65f);
            p.inputBg   = rgba(255,255,255,0.82f);
        } else {
            p.glassBg   = rgba(15,15,30,0.46f);
            p.glassBorde= rgba(255,255,255,0.18f);
            p.glassRim  = rgba(255,255,255,0.30f);
            p.panelSolid= rgba(20,20,40,0.65f);
            p.inputBg   = rgba(30,30,55,0.70f);
        }
    }

    p.fgDim = conAlfa(p.fg, 0.75f);
    return p;
}

} // namespace agenda
