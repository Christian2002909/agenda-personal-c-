// Quad a pantalla completa (triangulo unico generado por SV_VertexID) y copia
// simple de una textura. Este vertex shader (VSFull) lo reutilizan los pases de
// fondo y de desenfoque, ya que todos cubren la pantalla completa.

struct VSOut {
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

VSOut VSFull(uint id : SV_VertexID) {
    VSOut o;
    // Triangulo que cubre toda la pantalla: uv (0,0)-(2,2).
    o.uv  = float2((id << 1) & 2, id & 2);
    o.pos = float4(o.uv * float2(2, -2) + float2(-1, 1), 0, 1);
    return o;
}

Texture2D    tex0  : register(t0);
SamplerState samp0 : register(s0);

// Copia directa (blit) de una textura a la pantalla.
float4 PSCopy(VSOut i) : SV_Target {
    return tex0.Sample(samp0, i.uv);
}
