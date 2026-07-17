#pragma once
// Persistencia local en %APPDATA%\Agenda Personal\agenda-personal-data.json
// Equivale a electron/store.js. Es seguro para hilos (la UI y el scheduler
// acceden desde hilos distintos).

#include "app/Model.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>

namespace agenda {

class Store {
public:
    // Carga desde disco; si no existe, deja valores por defecto.
    void load();

    // ---- Tareas ----
    std::vector<Tarea> getTareas();
    // Inserta o actualiza por id y persiste. Devuelve la lista completa.
    std::vector<Tarea> saveTarea(const Tarea& tarea);
    // Borra definitivamente por id y persiste.
    std::vector<Tarea> deleteTarea(const std::string& id);

    // ---- Configuracion ----
    Config getConfig();
    Config saveConfig(const Config& cfg);

    // ---- Registro de avisos ya disparados ----
    std::unordered_map<std::string, int64_t> getUltimosAvisos();
    void marcarAvisoDisparado(const std::string& clave);

    // Ruta del archivo de datos (para mostrarla / diagnostico).
    std::wstring dataFilePath() const { return path_; }

private:
    void persistLocked();   // escribe todo el JSON (con el mutex ya tomado)
    void ensurePathLocked(); // calcula path_ y crea la carpeta si hace falta

    mutable std::mutex mtx_;
    std::vector<Tarea> tareas_;
    Config config_;
    std::unordered_map<std::string, int64_t> ultimosAvisos_;
    std::wstring path_;
    bool loaded_ = false;
};

} // namespace agenda
