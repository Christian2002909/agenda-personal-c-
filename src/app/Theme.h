#pragma once
// Paleta de colores resuelta a partir de la configuracion (tema + color de acento
// + tipo de fondo). Reproduce los valores de src/styles/themes.css y base.css del
// proyecto original. Los colores estan en formato lineal-ish 0..1 RGBA (sRGB sin
// premultiplicar) para consumirse tanto en ImGui como en los shaders.

#include "app/Model.h"

namespace agenda {

struct Col {
    float r = 0, g = 0, b = 0, a = 1;
};

struct ThemePalette {
    bool  isDark = false;

    Col   accent;       // color de acento principal
    Col   accent2;      // segundo tono del acento (degradados)

    Col   bg;           // color base del fondo
    Col   blobA;        // mancha radial 1 del degradado (accent 42%)
    Col   blobB;        // mancha radial 2 del degradado (accent2 36%)

    Col   fg;           // color de texto
    Col   fgDim;        // texto atenuado (opacidad ~0.75)

    Col   glassBg;      // tinte translucido del vidrio
    Col   glassBorde;   // borde del vidrio
    Col   glassRim;     // brillo especular del borde (rim)

    Col   panelSolid;   // fondo de botones secundarios / chips
    Col   inputBg;      // fondo de inputs
    Col   hoverBg;      // hover de botones de navegacion
    Col   borde;        // borde de inputs

    Col   badge;        // badge normal (= accent)
    Col   badgeUrgente; // #e0a02a
    Col   badgeVencida; // #e0554a
    Col   peligro;      // #e0554a (boton eliminar)

    bool  fondoConFoto = false; // hay foto/color solido detras (texto con sombra, vidrio mas opaco)
};

// Devuelve la paleta resuelta para una configuracion dada.
ThemePalette resolverPaleta(const Config& cfg);

// Lee la preferencia del sistema (tema claro/oscuro de Windows).
bool sistemaPrefiereOscuro();

// Resuelve "sistema" a "claro" u "oscuro".
bool temaEsOscuro(const Config& cfg);

} // namespace agenda
