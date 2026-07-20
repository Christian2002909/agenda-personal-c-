// Agenda Personal (nativa) — punto de entrada Win32 + Direct3D 11 + Dear ImGui.
// Integra: ventana, bandeja, scheduler de avisos, y el bucle de render con el
// efecto Liquid Glass detras de la interfaz.

#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>

#include "resource.h"
#include "gfx/Renderer.h"
#include "gfx/GlassRenderer.h"
#include "app/Store.h"
#include "app/Scheduler.h"
#include "app/Ui.h"
#include "app/Util.h"
#include "sys/Tray.h"
#include "sys/Autostart.h"
#include "sys/SingleInstance.h"
#include "net/Http.h"
#include "net/Email.h"
#include "net/GoogleSync.h"

#include <chrono>
#include <string>
#include <utility>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

using namespace agenda;

namespace {
constexpr UINT WM_APP_TRAY   = WM_APP + 1;
constexpr UINT WM_APP_NOTIFY = WM_APP + 2;
constexpr UINT ID_MENU_ABRIR = 1001;
constexpr UINT ID_MENU_SALIR = 1002;

Renderer      g_renderer;
GlassRenderer g_glass;
Store         g_store;
Scheduler     g_scheduler;
UiApp         g_ui;
Tray          g_tray;
HWND          g_hwnd = nullptr;
bool          g_saliendo = false;
bool          g_resize = false;
UINT          g_newW = 0, g_newH = 0;

void mostrarVentana() {
    if (!g_hwnd) return;
    ShowWindow(g_hwnd, SW_SHOW);
    if (IsIconic(g_hwnd)) ShowWindow(g_hwnd, SW_RESTORE);
    SetForegroundWindow(g_hwnd);
}
} // namespace

LRESULT WINAPI WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam)) return true;

    switch (msg) {
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED) {
            g_newW = LOWORD(lParam); g_newH = HIWORD(lParam); g_resize = true;
        }
        return 0;

    case WM_CLOSE:
        // Vive en la bandeja: cerrar oculta la ventana, no termina la app.
        ShowWindow(hwnd, SW_HIDE);
        return 0;

    case WM_APP_TRAY:
        if (LOWORD(lParam) == WM_LBUTTONUP || LOWORD(lParam) == WM_LBUTTONDBLCLK) {
            mostrarVentana();
        } else if (LOWORD(lParam) == WM_RBUTTONUP) {
            POINT pt; GetCursorPos(&pt);
            HMENU menu = CreatePopupMenu();
            AppendMenuW(menu, MF_STRING, ID_MENU_ABRIR, L"Abrir Agenda Personal");
            AppendMenuW(menu, MF_STRING, ID_MENU_SALIR, L"Salir");
            SetForegroundWindow(hwnd);
            UINT cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, hwnd, nullptr);
            DestroyMenu(menu);
            if (cmd == ID_MENU_ABRIR) mostrarVentana();
            else if (cmd == ID_MENU_SALIR) { g_saliendo = true; DestroyWindow(hwnd); }
        }
        return 0;

    case WM_APP_NOTIFY: {
        auto* pr = reinterpret_cast<std::pair<std::wstring, std::wstring>*>(lParam);
        if (pr) { g_tray.mostrarGlobo(pr->first, pr->second); delete pr; }
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// Envia una notificacion de globo desde cualquier hilo (via mensaje a la ventana).
static void notificar(const std::wstring& titulo, const std::wstring& cuerpo) {
    auto* pr = new std::pair<std::wstring, std::wstring>(titulo, cuerpo);
    PostMessageW(g_hwnd, WM_APP_NOTIFY, 0, reinterpret_cast<LPARAM>(pr));
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int) {
    // Una sola instancia.
    SingleInstance instancia;
    if (!instancia.adquirir()) { activarVentanaExistente(); return 0; }

    httpInit();
    g_store.load();

    // Identidad de la app (para que las notificaciones se muestren bien).
    SetCurrentProcessExplicitAppUserModelID(L"com.agendapersonal.app");

    // Ventana.
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_APPICON));
    wc.hIconSm = wc.hIcon;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = kWindowClass;
    RegisterClassExW(&wc);

    g_hwnd = CreateWindowExW(0, kWindowClass, L"Agenda Personal",
                             WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                             1100, 750, nullptr, nullptr, hInst, nullptr);
    if (!g_hwnd) return 1;

    if (!g_renderer.init(g_hwnd)) {
        wchar_t msg[256];
        swprintf_s(msg,
            L"No se pudo inicializar Direct3D 11 (ni siquiera en modo por software).\n"
            L"Codigo: 0x%08lX\n\n"
            L"Verifica que Windows tenga los controladores de video actualizados.",
            (unsigned long)g_renderer.ultimoError());
        MessageBoxW(g_hwnd, msg, L"Agenda Personal", MB_ICONERROR);
        return 1;
    }
    if (g_renderer.usandoRespaldoPorSoftware()) {
        MessageBoxW(g_hwnd,
            L"No se encontro una tarjeta grafica compatible con Direct3D 11 "
            L"(comun en maquinas virtuales o escritorio remoto).\n\n"
            L"La app abrira usando un renderizador por software: funciona "
            L"igual, pero se vera mas lenta.",
            L"Agenda Personal", MB_ICONINFORMATION);
    }

    if (!g_glass.init(g_renderer.device(), g_renderer.context())) {
        MessageBoxW(g_hwnd,
            L"No se pudieron compilar los shaders del efecto Liquid Glass.\n"
            L"Verifica que la carpeta 'shaders' este junto al ejecutable.",
            L"Agenda Personal", MB_ICONERROR);
        return 1;
    }

    // ImGui.
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;  // no escribir imgui.ini
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplDX11_Init(g_renderer.device(), g_renderer.context());

    // Fuente Segoe UI (incluye acentos del español en el rango por defecto).
    // Solo se carga si el archivo existe; si no, ImGui usa su fuente por defecto.
    if (GetFileAttributesA("C:\\Windows\\Fonts\\segoeui.ttf") != INVALID_FILE_ATTRIBUTES)
        io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 21.0f);

    // UI.
    g_ui.init(&g_store, g_renderer.device(), g_renderer.context(), g_hwnd);
    g_ui.onMostrarNotif = [](const std::wstring& t, const std::wstring& b) { g_tray.mostrarGlobo(t, b); };
    g_ui.onAutostart    = [](bool e) { establecerAutostart(e); };

    // Bandeja.
    g_tray.init(g_hwnd, WM_APP_TRAY, wc.hIcon);

    // Scheduler.
    g_scheduler.onNotificar = [](const std::wstring& t, const std::wstring& b) { notificar(t, b); };
    g_scheduler.onCorreo = [](const Tarea& tarea, int dias) {
        Config c = g_store.getConfig();
        if (c.email.direccion.empty() || c.email.appPassword.empty()) return;
        std::string asunto = dias > 0
            ? "Recordatorio: \"" + tarea.titulo + "\" vence en " + std::to_string(dias) + " dia(s)"
            : "Recordatorio: \"" + tarea.titulo + "\" vence hoy";
        std::string cuerpo = tarea.titulo + "\nFecha limite: " + tarea.fechaLimite + "\n\n" + tarea.notas;
        emailEnviar(c.email, asunto, cuerpo);
    };
    g_scheduler.onGoogleResync = [](const Tarea& tarea) {
        Config c = g_store.getConfig();
        if (!c.googleCalendar.activo) return;
        Tarea t = tarea; std::string eid;
        GoogleSync::crearOActualizarEvento(c.googleCalendar, t, eid);
        if (eid != t.googleEventId) { t.googleEventId = eid; g_store.saveTarea(t); }
        Config fresh = g_store.getConfig();
        fresh.googleCalendar.tokens = c.googleCalendar.tokens;
        g_store.saveConfig(fresh);
    };
    g_scheduler.start(&g_store);

    ShowWindow(g_hwnd, SW_SHOW);
    UpdateWindow(g_hwnd);

    auto t0 = std::chrono::high_resolution_clock::now();

    // Bucle principal.
    bool corriendo = true;
    while (corriendo) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) corriendo = false;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (!corriendo) break;

        if (g_resize) { g_renderer.resize(g_newW, g_newH); g_resize = false; }

        // Si la ventana esta oculta (en bandeja), no renderizamos: ahorra CPU/GPU.
        if (!IsWindowVisible(g_hwnd)) { Sleep(120); continue; }

        UINT w = g_renderer.width(), h = g_renderer.height();
        float timeSec = std::chrono::duration<float>(
            std::chrono::high_resolution_clock::now() - t0).count();

        io.DisplaySize = ImVec2((float)w, (float)h);

        // 1) Construir la interfaz (rellena los paneles glass de este frame).
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        g_ui.build((int)w, (int)h, timeSec);
        ImGui::Render();

        // 2) Fondo + desenfoque.
        ThemePalette pal = g_ui.paleta();
        int pw = 0, ph = 0;
        ID3D11ShaderResourceView* foto = g_ui.fotoFondo(pw, ph);
        ID3D11RenderTargetView* back = g_renderer.backRTV();
        g_glass.renderBackground(back, w, h, pal, g_ui.config(), foto, pw, ph);

        // 3) Paneles de vidrio (los registrados el frame anterior).
        for (const auto& p : g_ui.panelesPrevios())
            g_glass.drawPanel(back, w, h, p, timeSec);

        // 4) Interfaz ImGui encima del vidrio.
        g_renderer.bindBackBuffer();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_ui.endFrame();
        g_renderer.present(true);
    }

    // Salida ordenada.
    g_scheduler.stop();
    g_tray.quitar();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    g_renderer.shutdown();
    return 0;
}
