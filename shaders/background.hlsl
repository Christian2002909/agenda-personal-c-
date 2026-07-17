// Fondo de la app: degradado radial derivado del color de acento (equivale al
// body { background: radial-gradient(...) } de base.css) o una foto con ajuste
// "cover". Se dibuja a pantalla completa usando VSFull (fullscreen.hlsl).

struct VSOut {
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

cbuffer BgCB : register(b0) {
    float4 bgColor;      // color base
    float4 blobAColor;   // mancha radial 1 (rgb + alpha)
    float4 blobBColor;   // mancha radial 2
    float2 blobACenter;  // centro en uv (0..1)
    float2 blobBCenter;
    float2 blobARadius;  // radio en uv (x,y)
    float2 blobBRadius;
    float4 overlay;      // capa de oscurecido para fotos en tema oscuro (rgb + alpha)
    float2 resolution;   // tamaño del framebuffer en px
    float2 photoSize;    // tamaño de la foto en px (para "cover")
};

Texture2D    tex0  : register(t0);   // foto (solo modo foto)
SamplerState samp0 : register(s0);

// Contribucion de una mancha radial eliptica que va de color (centro) a
// transparente al 60% del radio, como los radial-gradient del CSS.
float blobMix(float2 uv, float2 center, float2 radius) {
    float2 d = (uv - center) / max(radius, 1e-4);
    float dist = length(d);
    return saturate(1.0 - dist / 0.6);
}

float4 PSGradient(VSOut i) : SV_Target {
    float3 col = bgColor.rgb;
    // Se componen B y luego A por encima (A es la capa superior en el CSS).
    col = lerp(col, blobBColor.rgb, blobBColor.a * blobMix(i.uv, blobBCenter, blobBRadius));
    col = lerp(col, blobAColor.rgb, blobAColor.a * blobMix(i.uv, blobACenter, blobARadius));
    return float4(col, 1.0);
}

float4 PSPhoto(VSOut i) : SV_Target {
    // Ajuste "cover": escala la foto para llenar la pantalla sin deformar.
    float screenAspect = resolution.x / max(resolution.y, 1.0);
    float photoAspect  = photoSize.x  / max(photoSize.y, 1.0);

    float2 uv = i.uv;
    if (photoAspect > screenAspect) {
        // La foto es mas ancha: recortar a los lados.
        float scale = screenAspect / photoAspect;
        uv.x = (uv.x - 0.5) * scale + 0.5;
    } else {
        // La foto es mas alta: recortar arriba/abajo.
        float scale = photoAspect / screenAspect;
        uv.y = (uv.y - 0.5) * scale + 0.5;
    }
    float3 col = tex0.Sample(samp0, uv).rgb;
    // Oscurecido opcional (tema oscuro sobre foto).
    col = lerp(col, overlay.rgb, overlay.a);
    return float4(col, 1.0);
}

// Color solido plano (para fondo tipo "color").
float4 PSSolid(VSOut i) : SV_Target {
    return float4(bgColor.rgb, 1.0);
}
