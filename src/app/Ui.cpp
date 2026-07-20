#include "app/Ui.h"
#include "app/Util.h"
#include "sys/FileDialog.h"
#include "sys/Autostart.h"
#include "net/GoogleSync.h"
#include "net/IcloudSync.h"
#include "net/Email.h"

#include <imgui.h>
#include <windows.h>
#include <shellapi.h>
#include <algorithm>
#include <thread>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <ctime>

namespace agenda {

static ImVec4 v4(const Col& c) { return ImVec4(c.r, c.g, c.b, c.a); }

// ---------------------------------------------------------------------------
// Selector de fecha (calendario emergente), en vez de escribir "AAAA-MM-DD" a mano.

static int diasEnMes(int anio, int mes) {
    static const int dias[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    int d = dias[mes-1];
    if (mes == 2 && ((anio%4==0 && anio%100!=0) || anio%400==0)) d = 29;
    return d;
}

// Dia de la semana (0=domingo..6=sabado) del dia 1 de ese mes.
static int diaSemanaPrimero(int anio, int mes) {
    std::tm t{}; t.tm_year=anio-1900; t.tm_mon=mes-1; t.tm_mday=1; t.tm_hour=12; t.tm_isdst=-1;
    std::time_t tt = std::mktime(&t);
    std::tm out{};
#if defined(_WIN32)
    localtime_s(&out, &tt);
#else
    localtime_r(&tt, &out);
#endif
    return out.tm_wday;
}

static const char* kMeses[] = {
    "Enero","Febrero","Marzo","Abril","Mayo","Junio",
    "Julio","Agosto","Septiembre","Octubre","Noviembre","Diciembre"
};
static const char* kDiasSemana[] = { "D","L","M","M","J","V","S" };

// Boton que muestra la fecha elegida y abre un calendario emergente para cambiarla.
// 'buf' contiene y recibe la fecha en formato "AAAA-MM-DD". Se dibuja con el
// mismo estilo que los demas campos de texto (no como un boton solido), con un
// icono de calendario a la derecha, para que se vea como un campo de fecha
// normal (igual que el <input type="date"> del original).
static void selectorFecha(const char* id, char* buf, size_t bufSize, const ThemePalette& pal) {
    int y=0, m=0, d=0;
    if (std::sscanf(buf, "%d-%d-%d", &y, &m, &d) != 3 || y < 1970) {
        std::string hoy = hoyLocalYmd();
        std::sscanf(hoy.c_str(), "%d-%d-%d", &y, &m, &d);
    }

    ImGui::PushID(id);
    // Mostrar en formato "dd/mm/aaaa" (visual) — internamente sigue "AAAA-MM-DD".
    std::string etiqueta = "dd/mm/aaaa";
    if (buf[0] && y > 0) {
        char fmtBuf[16];
        std::snprintf(fmtBuf, sizeof(fmtBuf), "%02d/%02d/%04d", d, m, y);
        etiqueta = fmtBuf;
    }
    // Mismo look que un campo de texto (fondo/borde de input), no un boton solido.
    ImGui::PushStyleColor(ImGuiCol_Button, v4(pal.inputBg));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, v4(pal.hoverBg));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, v4(pal.inputBg));
    ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
    bool abrir = ImGui::Button(etiqueta.c_str(), ImVec2(-1, 0));
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);

    // Icono de calendario a la derecha del campo.
    {
        ImVec2 mn = ImGui::GetItemRectMin();
        ImVec2 mx = ImGui::GetItemRectMax();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float s = 15.0f;
        ImVec2 iMin(mx.x - s - 12.0f, mn.y + (mx.y - mn.y - s) * 0.5f);
        ImVec2 iMax(iMin.x + s, iMin.y + s);
        ImU32 colIcon = ImGui::ColorConvertFloat4ToU32(v4(pal.fgDim));
        dl->AddRect(iMin, iMax, colIcon, 2.5f, 0, 1.3f);
        dl->AddLine(ImVec2(iMin.x, iMin.y + 4.5f), ImVec2(iMax.x, iMin.y + 4.5f), colIcon, 1.3f);
        dl->AddLine(ImVec2(iMin.x + s * 0.28f, iMin.y - 2.0f), ImVec2(iMin.x + s * 0.28f, iMin.y + 2.5f), colIcon, 1.5f);
        dl->AddLine(ImVec2(iMax.x - s * 0.28f, iMin.y - 2.0f), ImVec2(iMax.x - s * 0.28f, iMin.y + 2.5f), colIcon, 1.5f);
    }

    if (abrir) {
        ImGui::OpenPopup("calPopup");
        // Guarda el mes que se muestra al abrir (mes actual de la fecha elegida).
        ImGui::GetStateStorage()->SetInt(ImGui::GetID("navAnio"), y);
        ImGui::GetStateStorage()->SetInt(ImGui::GetID("navMes"), m);
    }
    // Ancho del popup ajustado EXACTO a las 7 columnas + relleno, para que la
    // grilla quede centrada y sin hueco a un lado. (Ademas evita el estiramiento
    // que causaba el SameLine(GetWindowWidth()-34).)
    const float kCell  = 30.0f;                 // ancho/columna de cada dia
    const float kGridW = 7.0f * kCell;          // ancho total de la grilla
    const float kPad   = ImGui::GetStyle().WindowPadding.x;
    const float kPopW  = kGridW + 2.0f * kPad;
    ImGui::SetNextWindowSizeConstraints(ImVec2(kPopW, 0), ImVec2(kPopW, 560));
    if (ImGui::BeginPopup("calPopup")) {
        // Fondo solido del calendario: evita que el contenido del modal de atras
        // se transparente a traves del popup.
        {
            ImDrawList* pdl = ImGui::GetWindowDrawList();
            ImVec2 a = ImGui::GetWindowPos();
            ImVec2 sz = ImGui::GetWindowSize();
            ImVec4 pf = pal.isDark ? ImVec4(0.14f,0.14f,0.18f,1.0f)
                                   : ImVec4(0.98f,0.98f,1.00f,1.0f);
            pdl->AddRectFilled(a, ImVec2(a.x+sz.x, a.y+sz.y),
                ImGui::ColorConvertFloat4ToU32(pf), ImGui::GetStyle().PopupRounding);
        }
        ImGuiStorage* st = ImGui::GetStateStorage();
        int navAnio = st->GetInt(ImGui::GetID("navAnio"), y);
        int navMes  = st->GetInt(ImGui::GetID("navMes"), m);

        const float x0 = ImGui::GetCursorPosX();  // borde izquierdo del contenido

        // --- Barra de mes: "<" a la izq, mes centrado, ">" a la der ---
        if (ImGui::SmallButton("<")) { navMes--; if (navMes < 1) { navMes = 12; navAnio--; } }
        char tit[40]; std::snprintf(tit, sizeof(tit), "%s %d", kMeses[navMes-1], navAnio);
        float tw = ImGui::CalcTextSize(tit).x;
        ImGui::SameLine(); ImGui::SetCursorPosX(x0 + (kGridW - tw) * 0.5f);
        ImGui::TextUnformatted(tit);
        float bw = ImGui::CalcTextSize(">").x + ImGui::GetStyle().FramePadding.x * 2.0f;
        ImGui::SameLine(); ImGui::SetCursorPosX(x0 + kGridW - bw);
        if (ImGui::SmallButton(">")) { navMes++; if (navMes > 12) { navMes = 1; navAnio++; } }
        st->SetInt(ImGui::GetID("navAnio"), navAnio);
        st->SetInt(ImGui::GetID("navMes"), navMes);

        ImGui::Spacing();
        // --- Encabezado de dias, centrado en cada columna de kCell ---
        for (int i = 0; i < 7; ++i) {
            float dw = ImGui::CalcTextSize(kDiasSemana[i]).x;
            ImGui::SetCursorPosX(x0 + i * kCell + (kCell - dw) * 0.5f);
            ImGui::TextDisabled("%s", kDiasSemana[i]);
            if (i < 6) ImGui::SameLine();
        }
        // (Sin NewLine: la primera fila de dias cae sola en la linea siguiente;
        //  un NewLine aqui metia una linea vacia extra y bajaba las fechas.)

        // --- Grilla de dias (cada fila arranca en x0, celdas de kCell) ---
        int primerDia = diaSemanaPrimero(navAnio, navMes);
        int totalDias = diasEnMes(navAnio, navMes);
        for (int i = 0; i < primerDia; ++i) {
            ImGui::Dummy(ImVec2(kCell, 30));
            ImGui::SameLine(0, 0);
        }
        for (int dia = 1; dia <= totalDias; ++dia) {
            bool esElegido = (dia == d && navMes == m && navAnio == y);
            char lbl[8]; std::snprintf(lbl, sizeof(lbl), "%d", dia);
            ImGui::PushID(dia);

            // Boton invisible solo para el clic/hover; el circulo de seleccion y
            // el numero se dibujan a mano, centrados por calculo exacto en la
            // celda (evita cualquier desalineacion del centrado automatico de
            // ImGui::Button con el estilo/redondeo global del resto de la app).
            bool clic = ImGui::InvisibleButton("dia", ImVec2(kCell, 30));
            ImVec2 cMin = ImGui::GetItemRectMin();
            ImVec2 cMax = ImGui::GetItemRectMax();
            ImVec2 centro((cMin.x + cMax.x) * 0.5f, (cMin.y + cMax.y) * 0.5f);
            ImDrawList* dl = ImGui::GetWindowDrawList();

            if (esElegido) {
                ImU32 colSel = ImGui::ColorConvertFloat4ToU32(ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
                dl->AddCircleFilled(centro, kCell * 0.42f, colSel, 24);
            } else if (ImGui::IsItemHovered()) {
                ImU32 colHov = ImGui::ColorConvertFloat4ToU32(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered));
                dl->AddCircleFilled(centro, kCell * 0.42f, colHov, 24);
            }
            ImVec2 tsz = ImGui::CalcTextSize(lbl);
            ImU32 colTxt = ImGui::ColorConvertFloat4ToU32(
                esElegido ? ImVec4(1,1,1,1) : ImGui::GetStyleColorVec4(ImGuiCol_Text));
            dl->AddText(ImVec2(centro.x - tsz.x * 0.5f, centro.y - tsz.y * 0.5f), colTxt, lbl);

            if (clic) {
                std::snprintf(buf, bufSize, "%04d-%02d-%02d", navAnio, navMes, dia);
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopID();
            if ((primerDia + dia) % 7 != 0) ImGui::SameLine(0, 0);
        }
        ImGui::EndPopup();
    }
    ImGui::PopID();
}

static int diasRestantes(const std::string& ymd) {
    int Y=0,M=0,D=0; if (std::sscanf(ymd.c_str(), "%d-%d-%d", &Y,&M,&D) != 3) return 0;
    std::tm lt{}; std::time_t now = std::time(nullptr);
#if defined(_WIN32)
    localtime_s(&lt, &now);
#else
    localtime_r(&now, &lt);
#endif
    std::tm hoy{}; hoy.tm_year=lt.tm_year; hoy.tm_mon=lt.tm_mon; hoy.tm_mday=lt.tm_mday; hoy.tm_hour=12; hoy.tm_isdst=-1;
    std::tm lim{}; lim.tm_year=Y-1900; lim.tm_mon=M-1; lim.tm_mday=D; lim.tm_hour=12; lim.tm_isdst=-1;
    double diff = std::difftime(std::mktime(&lim), std::mktime(&hoy));
    return (int)std::lround(diff / 86400.0);
}

// Pildora de estado (badge) dibujada con el draw list.
static void badge(const char* texto, const Col& color) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 pad(8, 3);
    ImVec2 ts = ImGui::CalcTextSize(texto);
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImVec2 p1(p0.x + ts.x + pad.x*2, p0.y + ts.y + pad.y*2);
    ImU32 bg = ImGui::ColorConvertFloat4ToU32(v4(color));
    dl->AddRectFilled(p0, p1, bg, (p1.y-p0.y)*0.5f);
    dl->AddText(ImVec2(p0.x+pad.x, p0.y+pad.y), IM_COL32(255,255,255,255), texto);
    ImGui::Dummy(ImVec2(ts.x + pad.x*2, ts.y + pad.y*2));
}

void UiApp::init(Store* store, ID3D11Device* dev, ID3D11DeviceContext* ctx, HWND hwnd) {
    store_ = store; dev_ = dev; ctx_ = ctx; hwnd_ = hwnd;
    config_ = store_->getConfig();
    tareas_ = store_->getTareas();
}

void UiApp::setEstado(const std::string& msg) {
    std::lock_guard<std::mutex> lk(estadoMtx_);
    estado_ = msg;
    estadoHasta_ = (double)nowEpochMs() + 6000.0;  // reloj propio (seguro entre hilos)
}

void UiApp::recargar() {
    tareas_ = store_->getTareas();
}

GlassPanel UiApp::panelBase(float x, float y, float w, float h, float radius) const {
    ThemePalette pal = resolverPaleta(config_);
    GlassPanel p; p.x=x; p.y=y; p.w=w; p.h=h; p.radius=radius;
    p.tint = pal.glassBg; p.rim = pal.glassRim;
    p.refractPx = 14.0f; p.shadowStrength = pal.isDark ? 0.28f : 0.18f;
    return p;
}

ID3D11ShaderResourceView* UiApp::fotoFondo(int& w, int& h) {
    const std::string& tipo = config_.fondo.tipo;
    std::wstring archivo;
    std::string clave = tipo + "|" + config_.fondo.valor;
    if (tipo == "lavanda")      archivo = resourcePath(L"assets\\fondo-lavanda.jpg");
    else if (tipo == "tulipanes") archivo = resourcePath(L"assets\\fondo-tulipanes.jpg");
    else if (tipo == "imagen" && !config_.fondo.valor.empty()) archivo = widen(config_.fondo.valor);
    else { fondoCargado_.clear(); fondoTex_.srv.Reset(); return nullptr; }

    if (clave != fondoCargado_) {
        fondoTex_ = cargarTexturaArchivo(dev_, ctx_, archivo);
        fondoCargado_ = clave;
    }
    if (!fondoTex_.valido()) return nullptr;
    w = fondoTex_.w; h = fondoTex_.h;
    return fondoTex_.srv.Get();
}

// ---------------------------------------------------------------------------
void UiApp::build(int width, int height, float timeSec) {
    timeSec_ = timeSec;
    panelesCur_.clear();

    // Aplicar actualizacion de Google llegada desde el hilo de conexion.
    {
        std::lock_guard<std::mutex> lk(pendMtx_);
        if (tieneGoogleNuevo_) { config_.googleCalendar = googleNuevo_; tieneGoogleNuevo_ = false; }
    }

    ThemePalette pal = resolverPaleta(config_);

    // Estilo ImGui derivado de la paleta.
    ImGuiStyle& st = ImGui::GetStyle();
    st.WindowRounding = 20; st.ChildRounding = 18; st.FrameRounding = 10;
    st.PopupRounding = 16; st.GrabRounding = 8; st.WindowBorderSize = 0;
    st.WindowPadding = ImVec2(16, 14); st.FramePadding = ImVec2(8, 5);
    st.ItemSpacing = ImVec2(8, 9);
    // Borde visible en campos/botones/popups: sin esto se pierden contra el
    // fondo translucido del vidrio (todo se veia "plano").
    st.FrameBorderSize = 1.0f;
    st.PopupBorderSize = 1.0f;
    ImVec4* c = st.Colors;
    c[ImGuiCol_Text]           = v4(pal.fg);
    c[ImGuiCol_WindowBg]       = ImVec4(0,0,0,0);
    c[ImGuiCol_ChildBg]        = ImVec4(0,0,0,0);
    c[ImGuiCol_Border]         = v4(pal.glassBorde);
    c[ImGuiCol_BorderShadow]   = ImVec4(0,0,0,0);
    c[ImGuiCol_TextDisabled]   = v4(pal.fgDim);
    // Los desplegables (combos) son popups reales: fondo opaco para que se lean.
    c[ImGuiCol_PopupBg]        = pal.isDark ? ImVec4(0.12f,0.12f,0.15f,1.00f)
                                            : ImVec4(0.98f,0.98f,1.00f,1.00f);
    c[ImGuiCol_FrameBg]        = v4(pal.inputBg);
    c[ImGuiCol_FrameBgHovered] = v4(pal.hoverBg);
    c[ImGuiCol_FrameBgActive]  = v4(pal.inputBg);
    c[ImGuiCol_Button]         = v4(pal.panelSolid);
    c[ImGuiCol_ButtonHovered]  = v4(pal.hoverBg);
    c[ImGuiCol_ButtonActive]   = v4(pal.accent);
    c[ImGuiCol_CheckMark]      = v4(pal.accent);
    c[ImGuiCol_SliderGrab]     = v4(pal.accent);
    c[ImGuiCol_Header]         = v4(pal.hoverBg);
    c[ImGuiCol_HeaderHovered]  = v4(pal.hoverBg);
    c[ImGuiCol_HeaderActive]   = v4(pal.accent);
    c[ImGuiCol_ScrollbarBg]    = ImVec4(0,0,0,0);

    barraNavegacion(width, height);

    // --- Zona de contenido ---
    const float pad = 16.0f;
    const float SIDEBAR_W = 160.0f, SIDEBAR_H = 66.0f;
    float cx=pad, cy=pad, cw=(float)width-2*pad, ch=(float)height-2*pad;
    const std::string& pos = config_.posicionPanel;
    if (pos == "izquierda") { cx = pad*2 + SIDEBAR_W; cw = (float)width - cx - pad; }
    else if (pos == "derecha") { cx = pad; cw = (float)width - SIDEBAR_W - pad*3; }
    else if (pos == "arriba") { cy = pad*2 + SIDEBAR_H; ch = (float)height - cy - pad; }
    else if (pos == "abajo") { cy = pad; ch = (float)height - SIDEBAR_H - pad*3; }

    ImGui::SetNextWindowPos(ImVec2(cx, cy), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(cw, ch), ImGuiCond_Always);
    ImGui::Begin("##contenido", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoBackground);

    if (vista_ == 0) vistaAgenda();
    else if (vista_ == 1) vistaHistorial();
    else vistaConfig();

    // Estado / toast.
    {
        std::lock_guard<std::mutex> lk(estadoMtx_);
        if (!estado_.empty() && (double)nowEpochMs() < estadoHasta_) {
            ImGui::Spacing();
            ImGui::TextWrapped("%s", estado_.c_str());
        }
    }

    ImGui::End();

    if (modalAbierto_) modalTarea();
}

void UiApp::endFrame() {
    panelesPrev_ = std::move(panelesCur_);
    panelesCur_.clear();
}

// ---------------------------------------------------------------------------
void UiApp::barraNavegacion(int width, int height) {
    ThemePalette pal = resolverPaleta(config_);
    const float pad = 16.0f;
    const float SIDEBAR_W = 160.0f, SIDEBAR_H = 66.0f;
    const std::string& pos = config_.posicionPanel;

    float x=pad, y=pad, w=SIDEBAR_W, h=(float)height-2*pad;
    bool horizontal = (pos == "arriba" || pos == "abajo");
    if (pos == "derecha") { x = (float)width - pad - SIDEBAR_W; }
    else if (pos == "arriba") { w=(float)width-2*pad; h=SIDEBAR_H; }
    else if (pos == "abajo")  { w=(float)width-2*pad; h=SIDEBAR_H; y=(float)height-pad-SIDEBAR_H; }

    registrarGlass(panelBase(x, y, w, h, 22.0f));

    ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(w, h), ImGuiCond_Always);
    ImGui::Begin("##panel", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoBackground);

    ImGui::PushStyleColor(ImGuiCol_Text, v4(pal.fg));
    ImGui::SetWindowFontScale(1.1f);
    ImGui::TextUnformatted("Agenda Personal");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();
    ImGui::Spacing();

    const char* etiquetas[3] = { "Tareas", "Historial", "Configuracion" };
    auto botonNav = [&](int idx) {
        bool activo = (vista_ == idx);
        // Igual que el original: inactivo = transparente (solo resalta al pasar
        // el mouse), activo = relleno solido con el color de acento.
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
        if (activo) {
            ImGui::PushStyleColor(ImGuiCol_Button, v4(pal.accent));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, v4(pal.accent));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, v4(pal.accent));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1,1,1,1));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0,0,0,0.04f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, v4(pal.hoverBg));
        }
        ImVec2 sz = horizontal ? ImVec2(120, 32) : ImVec2(w - 28, 32);
        if (ImGui::Button(etiquetas[idx], sz)) vista_ = idx;
        ImGui::PopStyleColor(activo ? 4 : 3);
        ImGui::PopStyleVar();
        if (horizontal) ImGui::SameLine();
    };
    for (int i = 0; i < 3; ++i) botonNav(i);

    ImGui::End();
}

// ---------------------------------------------------------------------------
void UiApp::vistaAgenda() {
    ThemePalette pal = resolverPaleta(config_);

    ImGui::SetWindowFontScale(1.3f);
    ImGui::TextUnformatted("Mis tareas");
    ImGui::SetWindowFontScale(1.0f);
    float btnNW = ImGui::CalcTextSize("+ Nueva tarea").x + 30.0f;
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - btnNW);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 16.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, v4(pal.accent));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1,1,1,1));
    if (ImGui::Button("+ Nueva tarea")) {
        modalAbierto_ = true; modalEsNueva_ = true;
        modalTarea_ = Tarea{};
        strncpy_s(inTitulo_, "", _TRUNCATE);
        strncpy_s(inFecha_, hoyLocalYmd().c_str(), _TRUNCATE);
        strncpy_s(inNotas_, "", _TRUNCATE);
        diasTemp_.clear(); horariosTemp_.clear();
        inNuevoDia_ = 3; strncpy_s(inNuevoHorario_, "09:00", _TRUNCATE);
    }
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
    ImGui::TextDisabled("Arrastra las tarjetas para reordenarlas.");
    ImGui::Spacing();

    // Tareas activas ordenadas.
    std::vector<Tarea> activas;
    for (auto& t : tareas_) if (!t.completada && !t.eliminada) activas.push_back(t);
    std::sort(activas.begin(), activas.end(), [](const Tarea& a, const Tarea& b){
        if (a.orden != b.orden) return a.orden < b.orden;
        return a.fechaLimite < b.fechaLimite;
    });

    if (activas.empty()) {
        ImGui::TextDisabled("No tienes tareas pendientes. Crea una con \"+ Nueva tarea\".");
        return;
    }

    float avail = ImGui::GetContentRegionAvail().x;
    int cols = std::max(1, (int)(avail / 300.0f));
    float gap = 14.0f;
    float cardW = (avail - gap * (cols - 1)) / cols;
    const float cardH = 226.0f; // +26 vs antes: espacio para el padding interno (24,20) de la tarjeta

    for (int i = 0; i < (int)activas.size(); ++i) {
        if (i % cols != 0) ImGui::SameLine(0, gap);
        ImGui::PushID(i);
        // Relleno interno generoso (igual que los paneles de Configuracion): el
        // WindowPadding global (16,14) se ve pegado a la esquina en una tarjeta;
        // AlwaysUseWindowPadding es necesario ademas para que un child SIN borde
        // respete el WindowPadding en vez de ignorarlo.
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 20));
        ImGui::BeginChild("card", ImVec2(cardW, cardH), ImGuiChildFlags_AlwaysUseWindowPadding,
                          ImGuiWindowFlags_NoScrollbar);
        // Registrar el vidrio con la posicion/tamaño reales del child.
        ImVec2 wp = ImGui::GetWindowPos(), ws = ImGui::GetWindowSize();
        registrarGlass(panelBase(wp.x, wp.y, ws.x, ws.y, 13.0f));

        dibujarTarjeta(activas[i], false, i);

        // Drag & drop para reordenar.
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
            ImGui::SetDragDropPayload("CARD", &i, sizeof(int));
            ImGui::TextUnformatted(activas[i].titulo.c_str());
            ImGui::EndDragDropSource();
        }
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("CARD")) {
                int from = *(const int*)p->Data;
                int to = i;
                if (from != to && from >= 0 && from < (int)activas.size()) {
                    // Reasignar 'orden' segun el nuevo orden visual.
                    Tarea movida = activas[from];
                    activas.erase(activas.begin()+from);
                    activas.insert(activas.begin()+to, movida);
                    for (int k = 0; k < (int)activas.size(); ++k) {
                        Tarea t = activas[k];
                        if (t.orden != k) { t.orden = k; store_->saveTarea(t); }
                    }
                    recargar();
                }
            }
            ImGui::EndDragDropTarget();
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopID();
    }
}

void UiApp::dibujarTarjeta(const Tarea& t, bool historial, int /*indice*/) {
    ThemePalette pal = resolverPaleta(config_);
    ImGui::PushStyleColor(ImGuiCol_Text, v4(pal.fg));

    // Cabecera: titulo + badge.
    ImGui::TextWrapped("%s", t.titulo.c_str());
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 90);
    if (historial) {
        if (t.eliminada) badge("Eliminada", pal.badgeVencida);
        else             badge("Completada", pal.badge);
    } else {
        int r = diasRestantes(t.fechaLimite);
        if (r < 0) badge("Vencida", pal.badgeVencida);
        else if (r == 0) badge("Hoy", pal.badgeUrgente);
        else { char b[32]; std::snprintf(b, sizeof(b), "%d dia(s)", r); badge(b, (r <= 1) ? pal.badgeUrgente : pal.badge); }
    }

    ImGui::PushStyleColor(ImGuiCol_Text, v4(pal.fgDim));
    ImGui::Text("Ultimo dia: %s", t.fechaLimite.c_str());
    if (!t.notas.empty()) ImGui::TextWrapped("%s", t.notas.c_str());
    ImGui::PopStyleColor();

    // Chips (dias antes / horarios).
    std::string chips;
    for (int d : t.avisosPrevios) chips += std::to_string(d) + "d antes   ";
    for (auto& h : t.horarios) chips += h + "   ";
    if (!chips.empty()) ImGui::TextDisabled("%s", chips.c_str());

    // Acciones abajo (deja el mismo margen inferior que el WindowPadding de la tarjeta).
    ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 72);
    if (historial) {
        if (ImGui::SmallButton("Reabrir")) {
            Tarea nt = t; nt.completada = false; nt.eliminada = false;
            nt.orden = 0;
            guardarTareaConSync(nt);
        }
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, v4(pal.peligro));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1,1,1,1));
        if (ImGui::SmallButton("Eliminar definitivamente")) eliminarDefinitiva(t.id);
        ImGui::PopStyleColor(2);
    } else {
        if (ImGui::SmallButton("Completar")) {
            Tarea nt = t; nt.completada = true; nt.completadaEn = nowIso();
            guardarTareaConSync(nt);
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Editar")) {
            modalAbierto_ = true; modalEsNueva_ = false; modalTarea_ = t;
            strncpy_s(inTitulo_, t.titulo.c_str(), _TRUNCATE);
            strncpy_s(inFecha_, t.fechaLimite.c_str(), _TRUNCATE);
            strncpy_s(inNotas_, t.notas.c_str(), _TRUNCATE);
            diasTemp_ = t.avisosPrevios; horariosTemp_ = t.horarios;
            inNuevoDia_ = 3; strncpy_s(inNuevoHorario_, "09:00", _TRUNCATE);
        }
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, v4(pal.peligro));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1,1,1,1));
        if (ImGui::SmallButton("Eliminar")) {
            Tarea nt = t; nt.eliminada = true; nt.eliminadaEn = nowIso();
            guardarTareaConSync(nt);
        }
        ImGui::PopStyleColor(2);
    }
    ImGui::PopStyleColor();
}

// ---------------------------------------------------------------------------
void UiApp::vistaHistorial() {
    ImGui::SetWindowFontScale(1.3f);
    ImGui::TextUnformatted("Historial");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::TextDisabled("Tareas completadas o eliminadas. Puedes reabrirlas o borrarlas para siempre.");
    ImGui::Spacing();

    std::vector<Tarea> hist;
    for (auto& t : tareas_) if (t.completada || t.eliminada) hist.push_back(t);
    std::sort(hist.begin(), hist.end(), [](const Tarea& a, const Tarea& b){
        const std::string& ka = !a.completadaEn.empty() ? a.completadaEn : a.eliminadaEn;
        const std::string& kb = !b.completadaEn.empty() ? b.completadaEn : b.eliminadaEn;
        return ka > kb;
    });

    if (hist.empty()) { ImGui::TextDisabled("Aun no hay nada en el historial."); return; }

    float avail = ImGui::GetContentRegionAvail().x;
    int cols = std::max(1, (int)(avail / 300.0f));
    float gap = 14.0f;
    float cardW = (avail - gap*(cols-1)) / cols;
    const float cardH = 204.0f; // +26 vs antes: espacio para el padding interno (24,20) de la tarjeta
    for (int i = 0; i < (int)hist.size(); ++i) {
        if (i % cols != 0) ImGui::SameLine(0, gap);
        ImGui::PushID(1000 + i);
        // Mismo relleno generoso que las tarjetas de "Mis tareas".
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 20));
        ImGui::BeginChild("hcard", ImVec2(cardW, cardH), ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoScrollbar);
        ImVec2 wp = ImGui::GetWindowPos(), ws = ImGui::GetWindowSize();
        registrarGlass(panelBase(wp.x, wp.y, ws.x, ws.y, 13.0f));
        dibujarTarjeta(hist[i], true, i);
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopID();
    }
}

// ---------------------------------------------------------------------------
static bool comboStr(const char* label, std::string& valor, const char* const* items, const char* const* valores, int n) {
    int idx = 0;
    for (int i = 0; i < n; ++i) if (valor == valores[i]) idx = i;
    bool changed = false;
    ImGui::TextUnformatted(label);
    std::string idLabel = std::string("##") + label;
    ImGui::PushItemWidth(-1);
    // BeginCombo sin flecha default para dibujar un chevron mas pequeno.
    if (ImGui::BeginCombo(idLabel.c_str(), items[idx], ImGuiComboFlags_NoArrowButton)) {
        for (int i = 0; i < n; ++i) {
            bool sel = (i == idx);
            if (ImGui::Selectable(items[i], sel)) { valor = valores[i]; changed = true; }
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    // Chevron pequeno (triangulo sutil) a la derecha del combo.
    {
        ImVec2 mn = ImGui::GetItemRectMin();
        ImVec2 mx = ImGui::GetItemRectMax();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        float midY = (mn.y + mx.y) * 0.5f;
        float cx = mx.x - 14.0f;
        float sz = 3.5f;
        ImU32 col = ImGui::ColorConvertFloat4ToU32(ImGui::GetStyleColorVec4(ImGuiCol_Text));
        dl->AddTriangleFilled(
            ImVec2(cx - sz, midY - sz * 0.5f),
            ImVec2(cx + sz, midY - sz * 0.5f),
            ImVec2(cx, midY + sz * 0.6f),
            col);
    }
    ImGui::PopItemWidth();
    return changed;
}

void UiApp::vistaConfig() {
    ThemePalette pal = resolverPaleta(config_);
    auto grupo = [&](const char* titulo, std::function<void()> cuerpo) {
        // Relleno interno generoso: que titulos/etiquetas/botones no queden
        // pegados a la esquina del panel (antes usaban el WindowPadding global 16,14).
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(28, 22));
        // AlwaysUseWindowPadding: sin esta bandera, un child SIN borde ignora el
        // WindowPadding y el contenido queda pegado a la esquina del panel.
        ImGui::BeginChild(titulo, ImVec2(540, 0),
            ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding,
            ImGuiWindowFlags_NoScrollbar);
        ImVec2 wp = ImGui::GetWindowPos(), ws = ImGui::GetWindowSize();
        registrarGlass(panelBase(wp.x, wp.y, ws.x, ws.y, 13.0f));
        // Titulo del panel mas grande/prominente y con aire debajo, como en la
        // version original (Electron).
        ImGui::SetWindowFontScale(1.30f);
        ImGui::TextUnformatted(titulo);
        ImGui::SetWindowFontScale(1.0f);
        ImGui::Dummy(ImVec2(0, 8)); // aire entre el titulo del panel y el primer campo
        cuerpo();
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::Dummy(ImVec2(0, 14)); // mas separacion visual entre paneles
    };

    ImGui::Dummy(ImVec2(0, 4)); // pequeno margen superior para el titulo de la pagina
    ImGui::SetWindowFontScale(1.45f);
    ImGui::TextUnformatted("Configuracion");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::Dummy(ImVec2(0, 6));

    ImGui::PushItemWidth(300);

    grupo("Apariencia", [&]{
        const char* temasN[] = {"Claro","Oscuro","Sistema"}; const char* temasV[] = {"claro","oscuro","sistema"};
        comboStr("Tema", config_.tema, temasN, temasV, 3);
        const char* colN[] = {"Normal","Verde","Coral","Naranja","Turquesa","Lila"};
        const char* colV[] = {"normal","verde","coral","naranja","turquesa","lila"};
        comboStr("Color del programa", config_.colorPrograma, colN, colV, 6);
        const char* posN[] = {"Arriba","Abajo","Izquierda","Derecha"};
        const char* posV[] = {"arriba","abajo","izquierda","derecha"};
        comboStr("Posicion del panel", config_.posicionPanel, posN, posV, 4);
        const char* fN[] = {"Degradado (por defecto)","Lavanda","Tulipanes","Color solido","Imagen"};
        const char* fV[] = {"degradado","lavanda","tulipanes","color","imagen"};
        comboStr("Fondo", config_.fondo.tipo, fN, fV, 5);
        if (config_.fondo.tipo == "color") {
            float col[3] = {1,1,1};
            unsigned int rgb = 0xffffff;
            if (config_.fondo.valor.size() >= 7 && config_.fondo.valor[0]=='#')
                rgb = (unsigned int)strtoul(config_.fondo.valor.c_str()+1, nullptr, 16);
            col[0]=((rgb>>16)&0xFF)/255.0f; col[1]=((rgb>>8)&0xFF)/255.0f; col[2]=(rgb&0xFF)/255.0f;
            if (ImGui::ColorEdit3("Color de fondo", col, ImGuiColorEditFlags_NoInputs)) {
                char hexs[8]; std::snprintf(hexs, sizeof(hexs), "#%02x%02x%02x",
                    (int)(col[0]*255), (int)(col[1]*255), (int)(col[2]*255));
                config_.fondo.valor = hexs;
            }
        }
        if (config_.fondo.tipo == "imagen") {
            if (ImGui::Button("Elegir imagen de fondo")) {
                std::wstring ruta = elegirImagen(hwnd_);
                if (!ruta.empty()) config_.fondo.valor = narrow(ruta);
            }
            if (!config_.fondo.valor.empty()) { ImGui::SameLine(); ImGui::TextDisabled("(elegida)"); }
        }
    });

    grupo("Canales de aviso", [&]{
        ImGui::Checkbox("Notificacion de Windows (se repite cada 5 min hasta completar)", &config_.notificaciones.ventana);
        ImGui::Checkbox("Aviso por correo electronico", &config_.notificaciones.correo);
        ImGui::TextUnformatted("Correccion horaria (minutos)");
        ImGui::PushItemWidth(-1);
        ImGui::InputInt("##correccionHoraria", &config_.correccionHorariaMin, 0, 0);
        ImGui::PopItemWidth();
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 500.0f);
        ImGui::TextDisabled("Si los avisos llegan tarde/temprano por zona horaria, ajusta aqui (ej. 60 o -60).");
        ImGui::PopTextWrapPos();
    });

    grupo("Avisos por correo", [&]{
        char dir[256]; strncpy_s(dir, config_.email.direccion.c_str(), _TRUNCATE);
        ImGui::TextUnformatted("Correo electronico");
        ImGui::PushItemWidth(-1);
        if (ImGui::InputText("##correoElectronico", dir, sizeof(dir))) config_.email.direccion = dir;
        ImGui::PopItemWidth();
        char pass[256]; strncpy_s(pass, config_.email.appPassword.c_str(), _TRUNCATE);
        ImGui::TextUnformatted("Contrasena de aplicacion");
        ImGui::PushItemWidth(-1);
        if (ImGui::InputText("##passEmail", pass, sizeof(pass), ImGuiInputTextFlags_Password)) config_.email.appPassword = pass;
        ImGui::PopItemWidth();
        char host[128]; strncpy_s(host, config_.email.smtpHost.c_str(), _TRUNCATE);
        ImGui::TextUnformatted("Servidor SMTP");
        ImGui::PushItemWidth(-1);
        if (ImGui::InputText("##smtpHost", host, sizeof(host))) config_.email.smtpHost = host;
        ImGui::PopItemWidth();
        ImGui::TextUnformatted("Puerto SMTP");
        ImGui::PushItemWidth(-1);
        ImGui::InputInt("##smtpPort", &config_.email.smtpPort, 0, 0);
        ImGui::PopItemWidth();
        if (ImGui::Button("Probar correo")) {
            guardarConfig(true);
            setEstado("Enviando correo de prueba...");
            EmailCfg ec = store_->getConfig().email;
            std::thread([this, ec]{
                std::string err = emailProbar(ec);
                setEstado(err.empty() ? "Correo enviado. Revisa tu bandeja (y Spam)." : err);
            }).detach();
        }
        ImGui::SameLine();
        if (ImGui::Button("Probar notificacion")) {
            if (onMostrarNotif) onMostrarNotif(L"Prueba de notificacion", L"Las notificaciones funcionan!");
        }
    });

    grupo("Inicio", [&]{
        if (ImGui::Checkbox("Iniciar Agenda Personal con Windows", &config_.iniciarConWindows)) {
            if (onAutostart) onAutostart(config_.iniciarConWindows);
        }
    });

    grupo("Sincronizacion con Google Calendar (opcional)", [&]{
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 500.0f);
        ImGui::TextDisabled("Este es el canal que llega a tu celular via Google Calendar.");
        ImGui::PopTextWrapPos();
        char cid[256]; strncpy_s(cid, config_.googleCalendar.clientId.c_str(), _TRUNCATE);
        ImGui::TextUnformatted("Client ID");
        ImGui::PushItemWidth(-1);
        if (ImGui::InputText("##clientId", cid, sizeof(cid))) config_.googleCalendar.clientId = cid;
        ImGui::PopItemWidth();
        char cs[256]; strncpy_s(cs, config_.googleCalendar.clientSecret.c_str(), _TRUNCATE);
        ImGui::TextUnformatted("Client Secret");
        ImGui::PushItemWidth(-1);
        if (ImGui::InputText("##clientSecret", cs, sizeof(cs))) config_.googleCalendar.clientSecret = cs;
        ImGui::PopItemWidth();
        if (ImGui::Button("Conectar con Google")) {
            guardarConfig(true);
            setEstado("Conectando... revisa el navegador");
            std::thread([this]{
                GoogleCfg g = store_->getConfig().googleCalendar;
                std::string err = GoogleSync::autenticar(g);
                if (err.empty()) {
                    Config fresh = store_->getConfig();
                    fresh.googleCalendar = g;
                    store_->saveConfig(fresh);
                    { std::lock_guard<std::mutex> lk(pendMtx_); tieneGoogleNuevo_ = true; googleNuevo_ = g; }
                    setEstado("Conectado con Google.");
                } else setEstado(err);
            }).detach();
        }
        ImGui::SameLine();
        ImGui::TextUnformatted(config_.googleCalendar.tokens.valido ? "Conectado" : "No conectado");
        ImGui::Checkbox("Activar sincronizacion con Google Calendar", &config_.googleCalendar.activo);
    });

    grupo("Sincronizacion con Apple Reminders / iCloud (opcional)", [&]{
        char aid[256]; strncpy_s(aid, config_.icloudReminders.appleId.c_str(), _TRUNCATE);
        ImGui::TextUnformatted("Apple ID");
        ImGui::PushItemWidth(-1);
        if (ImGui::InputText("##appleId", aid, sizeof(aid))) config_.icloudReminders.appleId = aid;
        ImGui::PopItemWidth();
        char ap[256]; strncpy_s(ap, config_.icloudReminders.appPassword.c_str(), _TRUNCATE);
        ImGui::TextUnformatted("Contrasena de aplicacion");
        ImGui::PushItemWidth(-1);
        if (ImGui::InputText("##passIcloud", ap, sizeof(ap), ImGuiInputTextFlags_Password)) config_.icloudReminders.appPassword = ap;
        ImGui::PopItemWidth();
        ImGui::Checkbox("Activar sincronizacion con iCloud", &config_.icloudReminders.activo);
    });

    ImGui::PopItemWidth();

    ImGui::PushStyleColor(ImGuiCol_Button, v4(pal.accent));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1,1,1,1));
    if (ImGui::Button("Guardar configuracion")) guardarConfig(false);
    ImGui::PopStyleColor(2);
}

// ---------------------------------------------------------------------------
void UiApp::modalTarea() {
    ThemePalette pal = resolverPaleta(config_);
    ImGuiIO& io = ImGui::GetIO();

    float mw = std::min(520.0f, io.DisplaySize.x * 0.92f);
    float mh = std::min(560.0f, io.DisplaySize.y * 0.9f);
    float mx = (io.DisplaySize.x - mw) * 0.5f;
    float my = (io.DisplaySize.y - mh) * 0.5f;
    registrarGlass(panelBase(mx, my, mw, mh, 24.0f));

    ImGui::SetNextWindowPos(ImVec2(mx, my), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(mw, mh), ImGuiCond_Always);
    ImGui::Begin("##modal", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

    // Oscurece la vista de atras y da al modal un fondo solido, para que el
    // texto de la pantalla de fondo no se transparente ni se superponga.
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->PushClipRectFullScreen();
        dl->AddRectFilled(ImVec2(0,0), io.DisplaySize, IM_COL32(0,0,0,130));
        dl->PopClipRect();
        ImVec4 fill = pal.isDark ? ImVec4(0.13f,0.13f,0.17f,0.96f)
                                 : ImVec4(0.97f,0.97f,1.00f,0.96f);
        dl->AddRectFilled(ImVec2(mx, my), ImVec2(mx+mw, my+mh),
                          ImGui::ColorConvertFloat4ToU32(fill), 24.0f);
    }

    ImGui::SetWindowFontScale(1.2f);
    ImGui::TextUnformatted(modalEsNueva_ ? "Nueva tarea" : "Editar tarea");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::Spacing();

    ImGui::TextUnformatted("Titulo");
    ImGui::PushItemWidth(-1);
    ImGui::InputTextWithHint("##titulo", "Que tienes que hacer?", inTitulo_, sizeof(inTitulo_));
    ImGui::TextUnformatted("Fecha limite (ultimo dia)");
    selectorFecha("fecha", inFecha_, sizeof(inFecha_), pal);
    ImGui::TextUnformatted("Notas");
    ImGui::InputTextMultiline("##notas", inNotas_, sizeof(inNotas_), ImVec2(-1, 70));
    ImGui::PopItemWidth();

    ImGui::Spacing();
    ImGui::TextUnformatted("Avisarme (dias antes del ultimo dia)");
    for (int i = 0; i < (int)diasTemp_.size(); ++i) {
        ImGui::PushID(2000+i);
        char b[32]; std::snprintf(b, sizeof(b), "%dd antes  X", diasTemp_[i]);
        if (ImGui::SmallButton(b)) { diasTemp_.erase(diasTemp_.begin()+i); ImGui::PopID(); break; }
        ImGui::PopID();
        if (i < (int)diasTemp_.size()-1) ImGui::SameLine();
    }
    ImGui::SetNextItemWidth(120);
    ImGui::InputInt("##nuevodia", &inNuevoDia_, 0, 0);
    ImGui::SameLine();
    if (ImGui::Button("+ Agregar##dias")) {
        if (inNuevoDia_ >= 0 && std::find(diasTemp_.begin(), diasTemp_.end(), inNuevoDia_) == diasTemp_.end())
            diasTemp_.push_back(inNuevoDia_);
    }

    ImGui::Spacing();
    ImGui::TextUnformatted("Horarios del aviso (HH:MM)");
    for (int i = 0; i < (int)horariosTemp_.size(); ++i) {
        ImGui::PushID(3000+i);
        std::string b = horariosTemp_[i] + "  X";
        if (ImGui::SmallButton(b.c_str())) { horariosTemp_.erase(horariosTemp_.begin()+i); ImGui::PopID(); break; }
        ImGui::PopID();
        if (i < (int)horariosTemp_.size()-1) ImGui::SameLine();
    }
    ImGui::SetNextItemWidth(120);
    ImGui::InputTextWithHint("##nuevohora", "09:00", inNuevoHorario_, sizeof(inNuevoHorario_));
    ImGui::SameLine();
    if (ImGui::Button("+ Agregar##horarios")) {
        std::string h = inNuevoHorario_;
        if (h.size() == 5 && h[2]==':' && std::find(horariosTemp_.begin(), horariosTemp_.end(), h) == horariosTemp_.end())
            horariosTemp_.push_back(h);
    }

    ImGui::Spacing(); ImGui::Spacing();
    // Alinear botones a la derecha (como en la version Electron).
    {
        const ImGuiStyle& sty = ImGui::GetStyle();
        float wCancel  = ImGui::CalcTextSize("Cancelar").x + sty.FramePadding.x * 2;
        float wGuardar = ImGui::CalcTextSize("Guardar").x  + sty.FramePadding.x * 2;
        float wElim    = modalEsNueva_ ? 0.0f : (ImGui::CalcTextSize("Eliminar").x + sty.FramePadding.x * 2);
        int   nGaps    = modalEsNueva_ ? 1 : 2;
        float totalW   = wCancel + wGuardar + wElim + sty.ItemSpacing.x * nGaps;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - totalW);
    }
    // Cancelar: ghost button (transparente, sin borde).
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, v4(pal.hoverBg));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, v4(pal.hoverBg));
    if (ImGui::Button("Cancelar")) modalAbierto_ = false;
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();
    ImGui::SameLine();
    if (!modalEsNueva_) {
        ImGui::PushStyleColor(ImGuiCol_Button, v4(pal.peligro));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1,1,1,1));
        if (ImGui::Button("Eliminar")) {
            Tarea nt = modalTarea_; nt.eliminada = true; nt.eliminadaEn = nowIso();
            guardarTareaConSync(nt);
            modalAbierto_ = false;
        }
        ImGui::PopStyleColor(2);
        ImGui::SameLine();
    }
    ImGui::PushStyleColor(ImGuiCol_Button, v4(pal.accent));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1,1,1,1));
    if (ImGui::Button("Guardar")) {
        std::string titulo = inTitulo_;
        std::string fecha = inFecha_;
        if (titulo.empty() || fecha.size() != 10) {
            setEstado("Completa al menos el titulo y la fecha limite (AAAA-MM-DD).");
        } else {
            Tarea t = modalTarea_;
            if (t.id.empty()) { t.id = generarId(); t.creadaEn = nowIso(); }
            t.titulo = titulo;
            t.fechaLimite = fecha;
            t.notas = inNotas_;
            t.avisosPrevios = diasTemp_;
            t.horarios = horariosTemp_;
            if (t.orden == 0 && modalEsNueva_) {
                int maxOrden = 0; for (auto& x : tareas_) maxOrden = std::max(maxOrden, x.orden);
                t.orden = maxOrden + 1;
            }
            guardarTareaConSync(t);
            modalAbierto_ = false;
        }
    }
    ImGui::PopStyleColor(2);

    ImGui::End();
}

// ---------------------------------------------------------------------------
void UiApp::guardarTareaConSync(Tarea t) {
    store_->saveTarea(t);
    recargar();

    // Sincronizacion en segundo plano (no bloquea la UI).
    std::thread([this, t]() mutable {
        Config c = store_->getConfig();
        std::string err;
        bool seCompletoOElimino = (t.completada || t.eliminada) && !t.googleEventId.empty();

        if (seCompletoOElimino) {
            err = GoogleSync::eliminarEvento(c.googleCalendar, t);
            t.googleEventId.clear();
            store_->saveTarea(t);
        } else if (c.googleCalendar.activo) {
            std::string eid;
            err = GoogleSync::crearOActualizarEvento(c.googleCalendar, t, eid);
            if (eid != t.googleEventId) { t.googleEventId = eid; store_->saveTarea(t); }
            // Persistir tokens refrescados sin pisar otras ediciones.
            Config fresh = store_->getConfig();
            fresh.googleCalendar.tokens = c.googleCalendar.tokens;
            store_->saveConfig(fresh);
        }
        if (!err.empty()) setEstado(err);

        if (c.icloudReminders.activo) {
            std::string e2 = IcloudSync::sincronizarTarea(c.icloudReminders, t);
            if (!e2.empty()) setEstado(e2);
        }
    }).detach();
}

void UiApp::eliminarDefinitiva(const std::string& id) {
    store_->deleteTarea(id);
    recargar();
}

void UiApp::guardarConfig(bool silencioso) {
    config_ = store_->saveConfig(config_);
    if (onAutostart) onAutostart(config_.iniciarConWindows);
    if (!silencioso) setEstado("Configuracion guardada.");
}

} // namespace agenda
