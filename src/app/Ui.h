#pragma once
// Interfaz con Dear ImGui: vistas de tareas, historial, configuracion y el modal
// de tarea. Reproduce app.js + index.html del proyecto original. Los paneles de
// vidrio se registran por rectangulo y los dibuja GlassRenderer (con 1 frame de
// desfase, imperceptible) para que el efecto quede detras del contenido ImGui.

#include "app/Model.h"
#include "app/Store.h"
#include "app/Theme.h"
#include "gfx/GlassRenderer.h"
#include "gfx/Texture.h"

#include <d3d11.h>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace agenda {

class UiApp {
public:
    void init(Store* store, ID3D11Device* dev, ID3D11DeviceContext* ctx, HWND hwnd);

    // Construye la interfaz del frame (debe llamarse entre ImGui::NewFrame y Render).
    void build(int width, int height, float timeSec);
    // Mueve los paneles de este frame a "previo" (para dibujarlos el siguiente frame).
    void endFrame();

    // Paneles de vidrio a dibujar ESTE frame (los que se registraron el frame anterior).
    const std::vector<GlassPanel>& panelesPrevios() const { return panelesPrev_; }

    ThemePalette paleta() const { return resolverPaleta(config_); }
    const Config& config() const { return config_; }

    // Textura de foto de fondo segun la config (se carga bajo demanda). Puede ser null.
    ID3D11ShaderResourceView* fotoFondo(int& w, int& h);

    // Callbacks provistos por main (acciones del sistema).
    std::function<void(const std::wstring&, const std::wstring&)> onMostrarNotif; // probar notificacion
    std::function<void(bool)> onAutostart;

    // Mensaje temporal (toast) seguro para hilos.
    void setEstado(const std::string& msg);

private:
    // Vistas.
    void barraNavegacion(int width, int height);
    void vistaAgenda();
    void vistaHistorial();
    void vistaConfig();
    void modalTarea();
    void dibujarTarjeta(const Tarea& t, bool historial, int indice);
    // Tarjetas flotantes (ventanas libres arrastrables) de tareas/historial.
    void dibujarTarjetasFlotantes(bool historial);

    // Operaciones de datos (+ sincronizacion en segundo plano).
    void recargar();
    void guardarTareaConSync(Tarea t);
    void eliminarDefinitiva(const std::string& id);
    void guardarConfig(bool silencioso);

    void registrarGlass(const GlassPanel& p) { panelesCur_.push_back(p); }
    GlassPanel panelBase(float x, float y, float w, float h, float radius) const;

    Store* store_ = nullptr;
    ID3D11Device* dev_ = nullptr;
    ID3D11DeviceContext* ctx_ = nullptr;
    HWND hwnd_ = nullptr;

    Config config_;
    std::vector<Tarea> tareas_;

    int vista_ = 0;  // 0=agenda 1=historial 2=config
    float timeSec_ = 0.0f;

    // Modal.
    bool modalAbierto_ = false;
    Tarea modalTarea_;
    bool modalEsNueva_ = true;
    char inTitulo_[256] = "";
    char inFecha_[16] = "";
    char inNotas_[1024] = "";
    int  inNuevoDia_ = 3;
    char inNuevoHorario_[8] = "";
    std::vector<int> diasTemp_;
    std::vector<std::string> horariosTemp_;

    // Fondo cacheado.
    LoadedTexture fondoTex_;
    std::string fondoCargado_;   // clave del fondo ya cargado (tipo|valor)

    // Estado/toast.
    std::mutex estadoMtx_;
    std::string estado_;
    double estadoHasta_ = 0.0;

    // Paneles de vidrio.
    std::vector<GlassPanel> panelesCur_;
    std::vector<GlassPanel> panelesPrev_;

    // Reordenar por arrastre libre: se arrastra la tarjeta suelta y al soltar se
    // reubica en la posicion (slot) mas cercana, corriendo a las demas.
    std::string dragId_;                     // id de la tarjeta que se arrastra ("" = ninguna)
    float dragGrabX_ = 0.0f, dragGrabY_ = 0.0f;  // punto de agarre dentro de la tarjeta

    // Scroll suave (estilo Apple) de la ventana de contenido.
    float scrollTarget_      = 0.0f;   // destino al que se acerca el scroll
    float scrollLastApplied_ = 0.0f;   // ultimo valor que fijamos (para detectar la barra)

    // Rectangulo del area de contenido (para ubicar/clampear las tarjetas libres).
    float contentX_ = 0.0f, contentY_ = 0.0f, contentW_ = 0.0f, contentH_ = 0.0f;

    // Actualizacion de conexion Google llegada desde el hilo de autenticacion.
    std::mutex pendMtx_;
    bool tieneGoogleNuevo_ = false;
    GoogleCfg googleNuevo_;
};

} // namespace agenda
