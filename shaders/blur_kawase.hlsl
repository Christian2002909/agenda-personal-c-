// Desenfoque Dual Kawase: un blur gaussiano aproximado muy barato en GPU
// (equivalente a backdrop-filter: blur() del CSS). Se hace en dos fases:
//   - PSDown: reduce a la mitad muestreando 5 puntos.
//   - PSUp:   amplia al doble muestreando 8 puntos.
// Encadenando varias reducciones y ampliaciones se logra un desenfoque amplio
// con muy pocas muestras, ideal para mantener la app rapida.

struct VSOut {
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

cbuffer BlurCB : register(b0) {
    float2 texelSize;  // 1 / tamaño de la textura de origen
    float2 _pad;
};

Texture2D    tex0  : register(t0);
SamplerState samp0 : register(s0);

float4 PSDown(VSOut i) : SV_Target {
    float2 uv = i.uv;
    float2 h = texelSize; // medio pixel del destino ~ un pixel del origen

    float4 sum = tex0.Sample(samp0, uv) * 4.0;
    sum += tex0.Sample(samp0, uv + float2(-h.x, -h.y));
    sum += tex0.Sample(samp0, uv + float2( h.x, -h.y));
    sum += tex0.Sample(samp0, uv + float2(-h.x,  h.y));
    sum += tex0.Sample(samp0, uv + float2( h.x,  h.y));
    return sum / 8.0;
}

float4 PSUp(VSOut i) : SV_Target {
    float2 uv = i.uv;
    float2 h = texelSize;

    float4 sum = float4(0,0,0,0);
    sum += tex0.Sample(samp0, uv + float2(-h.x * 2.0, 0.0));
    sum += tex0.Sample(samp0, uv + float2(-h.x, h.y)) * 2.0;
    sum += tex0.Sample(samp0, uv + float2(0.0,  h.y * 2.0));
    sum += tex0.Sample(samp0, uv + float2( h.x, h.y)) * 2.0;
    sum += tex0.Sample(samp0, uv + float2( h.x * 2.0, 0.0));
    sum += tex0.Sample(samp0, uv + float2( h.x, -h.y)) * 2.0;
    sum += tex0.Sample(samp0, uv + float2(0.0, -h.y * 2.0));
    sum += tex0.Sample(samp0, uv + float2(-h.x, -h.y)) * 2.0;
    return sum / 12.0;
}
