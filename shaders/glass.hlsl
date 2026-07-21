// Material "Liquid Glass" estilo Apple para un panel rectangular redondeado.
// Dibuja un quad (posicionado por GlassCB) y, por pixel:
//   1) refracta el fondo desenfocado (blurTex) doblando la luz cerca del borde,
//   2) aplica un tinte translucido (equivale a --glass-bg),
//   3) añade un brillo especular en el borde superior-izquierdo (rim),
//   4) suma un reflejo diagonal en movimiento (equivale a la animacion sheen),
//   5) proyecta una sombra flotante suave por debajo.
// El quad se dibuja algo mas grande que el panel para dejar sitio a la sombra.

cbuffer GlassCB : register(b0) {
    float2 resolution;    // tamaño del framebuffer en px
    float2 panelMin;      // esquina sup-izq del panel en px
    float2 panelSize;     // ancho/alto del panel en px
    float2 quadMin;       // esquina del quad (panel + margen de sombra)
    float2 quadSize;      // tamaño del quad
    float4 tint;          // color del vidrio (rgb + alpha de translucidez)
    float4 rim;           // color del brillo de borde (rgb + intensidad)
    float  radius;        // radio de esquina en px
    float  time;          // segundos, para animar el sheen
    float  refractPx;     // fuerza de refraccion en px
    float  shadowStrength;// opacidad maxima de la sombra
};

struct VSOut {
    float4 pos       : SV_POSITION;
    float2 screenPx  : TEXCOORD0;  // posicion del pixel en px
};

// Vertex shader: genera un quad (2 triangulos, triangle strip de 4 vertices)
// que cubre quadMin..quadMin+quadSize, convertido a clip space.
VSOut VSGlass(uint id : SV_VertexID) {
    float2 corner = float2((id == 1 || id == 3) ? 1.0 : 0.0,
                           (id >= 2)            ? 1.0 : 0.0);
    float2 px = quadMin + corner * quadSize;
    float2 ndc = float2(px.x / resolution.x * 2.0 - 1.0,
                        1.0 - px.y / resolution.y * 2.0);
    VSOut o;
    o.pos = float4(ndc, 0, 1);
    o.screenPx = px;
    return o;
}

Texture2D    blurTex     : register(t0);   // fondo desenfocado (pantalla completa)
Texture2D    bgSharpTex  : register(t1);   // fondo NITIDO (para el look "agua pura")
SamplerState samp0       : register(s0);

// SDF de rectangulo redondeado: <0 dentro, 0 en el borde, >0 fuera. 'p' es la
// posicion relativa al centro; 'b' es media dimension.
float sdRoundRect(float2 p, float2 b, float r) {
    float2 q = abs(p) - (b - r);
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

float4 PSGlass(VSOut i) : SV_Target {
    float2 p        = i.screenPx;
    float2 center   = panelMin + panelSize * 0.5;
    float2 mitad    = panelSize * 0.5;
    float  r        = min(radius, min(mitad.x, mitad.y));

    float d = sdRoundRect(p - center, mitad, r);

    // Normal 2D del borde (gradiente de la SDF por diferencias finitas).
    float e = 1.0;
    float dx = sdRoundRect(p + float2(e,0) - center, mitad, r) - sdRoundRect(p - float2(e,0) - center, mitad, r);
    float dy = sdRoundRect(p + float2(0,e) - center, mitad, r) - sdRoundRect(p - float2(0,e) - center, mitad, r);
    float2 n = normalize(float2(dx, dy) + 1e-5);

    // --- Sombra flotante (fuera del panel, desplazada hacia abajo) ---
    float2 shadowCenter = center + float2(0.0, 12.0);
    float dShadow = sdRoundRect(p - shadowCenter, mitad, r);
    float shadow = exp(-max(dShadow, 0.0) * max(dShadow, 0.0) / (2.0 * 22.0 * 22.0)) * shadowStrength;

    // --- Cobertura del panel (antialias de 1px en el borde) ---
    float cov = saturate(0.5 - d);

    // Fuera del panel: solo sombra.
    if (cov <= 0.001) {
        return float4(0.0, 0.0, 0.0, shadow * (1.0 - cov));
    }

    // --- Refraccion "agua pura" (lente de domo, eta = 1/1.33) ---------------
    // Se trata el panel como un domo: plano en el centro y curvado hacia el
    // borde. La normal del domo apunta radialmente hacia afuera y crece con la
    // distancia al centro (curvature=1.0), asi la luz se dobla suave en el
    // centro y mas fuerte cerca del borde (magnificacion tipo lente de agua).
    float2 local = (p - center) / mitad;               // -1..1 dentro del panel
    float  rr    = saturate(length(local));
    float  curvature = pow(rr, 1.0);                    // agua pura: curvature 1.0
    float2 domeN = normalize(local + 1e-5) * curvature;
    // Normal 3D del domo (el factor controla la inclinacion ~ edgeSharpness).
    float3 N3 = normalize(float3(domeN * 2.1, 1.0));
    float3 I3 = float3(0.0, 0.0, -1.0);                 // rayo de vista hacia la pantalla
    float3 R3 = refract(I3, N3, 1.0 / 1.33);           // eta del agua
    float2 uv = p / resolution;
    float2 refr = R3.xy * 0.07;                         // distortionStrength (mas "liquid glass")

    // Agua = clara: mayormente NITIDO con un toque de desenfoque (no esmerilado).
    float3 sharp   = bgSharpTex.Sample(samp0, uv + refr).rgb;
    float3 blurred = blurTex.Sample(samp0, uv + refr * 0.6).rgb;
    float3 bg = lerp(sharp, blurred, 0.35);

    // Tinte translucido del vidrio (suave, para no perder legibilidad).
    float3 col = lerp(bg, tint.rgb, tint.a);

    // Ligero degradado vertical de luz (arriba mas claro), como el CSS.
    float vert = saturate(1.0 - (p.y - panelMin.y) / max(panelSize.y, 1.0));
    col += vert * 0.06;

    // Rim especular: brillo fino en el borde que mira hacia arriba-izquierda.
    float rimMask = smoothstep(3.0, 0.0, abs(d));
    float facing  = saturate(dot(n, normalize(float2(-1.0, -1.0))));
    col += rim.rgb * rim.a * rimMask * facing;

    // (Sheen animado eliminado: se prefiere el vidrio "liquido" sin el brillo
    // diagonal que se desplazaba y distraia.)

    return float4(col, cov);
}
