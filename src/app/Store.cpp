#include "app/Store.h"
#include "app/Util.h"

#include <windows.h>
#include <shlobj.h>
#include <fstream>
#include <sstream>
#include <algorithm>

using nlohmann::json;

namespace agenda {

void Store::ensurePathLocked() {
    if (!path_.empty()) return;

    wchar_t buf[MAX_PATH] = L".";
    // CSIDL_APPDATA = %APPDATA% (Roaming). Fiable en todas las versiones de Windows.
    SHGetFolderPathW(nullptr, CSIDL_APPDATA | CSIDL_FLAG_CREATE, nullptr, 0, buf);
    std::wstring base = buf;
    if (base.empty()) base = L".";

    std::wstring dir = base + L"\\Agenda Personal";
    CreateDirectoryW(dir.c_str(), nullptr);   // no falla si ya existe
    path_ = dir + L"\\agenda-personal-data.json";
}

void Store::load() {
    std::lock_guard<std::mutex> lk(mtx_);
    ensurePathLocked();
    loaded_ = true;

    std::ifstream in(path_.c_str(), std::ios::binary);
    if (!in) {
        // Primera ejecucion: se queda con los valores por defecto de los structs.
        return;
    }
    std::stringstream ss;
    ss << in.rdbuf();
    const std::string contenido = ss.str();
    if (contenido.empty()) return;

    try {
        json j = json::parse(contenido);
        if (j.contains("tareas"))  tareas_  = j.at("tareas").get<std::vector<Tarea>>();
        if (j.contains("config"))  config_  = j.at("config").get<Config>();
        if (j.contains("ultimosAvisos"))
            ultimosAvisos_ = j.at("ultimosAvisos").get<std::unordered_map<std::string, int64_t>>();
    } catch (const std::exception&) {
        // JSON corrupto: se ignora y se arranca limpio (no se pierde el archivo
        // hasta el proximo guardado).
    }
}

void Store::persistLocked() {
    ensurePathLocked();
    json j;
    j["tareas"]        = tareas_;
    j["config"]        = config_;
    j["ultimosAvisos"] = ultimosAvisos_;

    const std::string texto = j.dump(2);

    // Escritura atomica: temporal + rename, para no corromper el archivo si
    // la app se cierra a la mitad.
    std::wstring tmp = path_ + L".tmp";
    {
        std::ofstream out(tmp.c_str(), std::ios::binary | std::ios::trunc);
        if (!out) return;
        out.write(texto.data(), static_cast<std::streamsize>(texto.size()));
    }
    MoveFileExW(tmp.c_str(), path_.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
}

std::vector<Tarea> Store::getTareas() {
    std::lock_guard<std::mutex> lk(mtx_);
    return tareas_;
}

std::vector<Tarea> Store::saveTarea(const Tarea& tarea) {
    std::lock_guard<std::mutex> lk(mtx_);
    bool encontrado = false;
    for (auto& t : tareas_) {
        if (t.id == tarea.id) { t = tarea; encontrado = true; break; }
    }
    if (!encontrado) tareas_.push_back(tarea);
    persistLocked();
    return tareas_;
}

std::vector<Tarea> Store::deleteTarea(const std::string& id) {
    std::lock_guard<std::mutex> lk(mtx_);
    tareas_.erase(std::remove_if(tareas_.begin(), tareas_.end(),
                                 [&](const Tarea& t) { return t.id == id; }),
                  tareas_.end());
    persistLocked();
    return tareas_;
}

Config Store::getConfig() {
    std::lock_guard<std::mutex> lk(mtx_);
    return config_;
}

Config Store::saveConfig(const Config& cfg) {
    std::lock_guard<std::mutex> lk(mtx_);
    config_ = cfg;
    persistLocked();
    return config_;
}

std::unordered_map<std::string, int64_t> Store::getUltimosAvisos() {
    std::lock_guard<std::mutex> lk(mtx_);
    return ultimosAvisos_;
}

void Store::marcarAvisoDisparado(const std::string& clave) {
    std::lock_guard<std::mutex> lk(mtx_);
    ultimosAvisos_[clave] = nowEpochMs();
    persistLocked();
}

} // namespace agenda
