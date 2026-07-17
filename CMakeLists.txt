cmake_minimum_required(VERSION 3.20)

# Fuerza generar en 64 bits (por defecto los generadores de Visual Studio usan
# Win32 si no se especifica, lo que produciria un .exe de 32 bits incompatible
# con las DLLs de 64 bits que instala vcpkg). Debe fijarse antes de project().
if(NOT CMAKE_GENERATOR_PLATFORM)
  set(CMAKE_GENERATOR_PLATFORM x64)
endif()

project(AgendaPersonal LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Agenda Personal (nativa) — C++ / Direct3D 11 / Dear ImGui con efecto Liquid Glass
# Port nativo de la app Electron original, pensado para ser mucho mas rapido y ligero.
# Solo compila en Windows (usa Win32 + Direct3D 11).
# ---------------------------------------------------------------------------

if(NOT WIN32)
  message(FATAL_ERROR "Agenda Personal nativa solo compila en Windows (Win32 + Direct3D 11).")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
  add_compile_options(/utf-8 /W3 /EHsc)
  add_compile_definitions(UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
endif()

include(FetchContent)

# --- Dear ImGui (sin CMake propio: se compila como libreria estatica) ---------
FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG        v1.91.5
)
FetchContent_MakeAvailable(imgui)

add_library(imgui STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
)
target_include_directories(imgui PUBLIC
  ${imgui_SOURCE_DIR}
  ${imgui_SOURCE_DIR}/backends
)

# --- nlohmann/json ------------------------------------------------------------
FetchContent_Declare(
  json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        v3.11.3
)
FetchContent_MakeAvailable(json)

# --- stb (solo cabeceras, para cargar imagenes) -------------------------------
FetchContent_Declare(
  stb
  GIT_REPOSITORY https://github.com/nothings/stb.git
  GIT_TAG        master
)
FetchContent_MakeAvailable(stb)

# --- libcurl (via vcpkg: correo SMTP + Google Calendar + iCloud CalDAV) --------
find_package(CURL REQUIRED)

# --- Ejecutable de la app -----------------------------------------------------
file(GLOB_RECURSE AGENDA_SOURCES CONFIGURE_DEPENDS src/*.cpp)

add_executable(AgendaPersonal WIN32 ${AGENDA_SOURCES} app.rc)

target_include_directories(AgendaPersonal PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${stb_SOURCE_DIR}
)

target_link_libraries(AgendaPersonal PRIVATE
  imgui
  nlohmann_json::nlohmann_json
  CURL::libcurl
  d3d11
  dxgi
  d3dcompiler
  dwmapi
  windowscodecs
  shell32
  ole32
  comdlg32
  ws2_32
  advapi32
  user32
  gdi32
)

# --- Copiar recursos (imagenes y shaders) junto al .exe -----------------------
add_custom_command(TARGET AgendaPersonal POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_CURRENT_SOURCE_DIR}/assets  $<TARGET_FILE_DIR:AgendaPersonal>/assets
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_CURRENT_SOURCE_DIR}/shaders $<TARGET_FILE_DIR:AgendaPersonal>/shaders
  COMMENT "Copiando assets y shaders junto al ejecutable"
)
