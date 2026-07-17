#pragma once
// Utilidades pequeñas compartidas: generacion de IDs, marcas de tiempo y fechas.

#include <string>
#include <cstdint>

namespace agenda {

// Genera un id unico tipo "t-<hex>" (equivalente a crypto.randomUUID del original).
std::string generarId();

// Marca de tiempo ISO 8601 en UTC (equivalente a Date.toISOString()).
std::string nowIso();

// Epoch actual en milisegundos.
int64_t nowEpochMs();

// Fecha de hoy en horario local como "YYYY-MM-DD".
std::string hoyLocalYmd();

// Conversion UTF-8 <-> UTF-16 (Windows).
std::wstring widen(const std::string& s);
std::string  narrow(const std::wstring& s);

} // namespace agenda
