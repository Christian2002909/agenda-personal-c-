# Agenda Personal — versión nativa C++ / Direct3D 11 (Liquid Glass)

Reescritura **nativa de Windows** de la app Electron
[`agenda_personal`](https://github.com/Christian2002909/agenda_personal), hecha en
**C++ + Direct3D 11 + Dear ImGui**, con el efecto **Liquid Glass** de Apple
implementado de verdad en HLSL (desenfoque *Dual Kawase* + refracción + brillo de
borde + reflejo en movimiento).

El objetivo es que sea **mucho más rápida y ligera** que la versión Electron:
un único `.exe` pequeño, sin navegador embebido, con render por GPU.

Conserva **todas las funciones** del original:

- Tareas con fecha límite, aviso *N días antes* (varios), varios horarios y notas.
- Historial (completar / eliminar / reabrir / borrar definitivamente).
- Arrastrar para reordenar.
- Notificaciones de Windows (se repiten cada 5 min hasta completar).
- Avisos por **correo** (SMTP).
- Sincronización opcional con **Google Calendar** (OAuth) y **Apple Reminders / iCloud** (CalDAV).
- Apariencia: tema claro/oscuro/sistema, 6 colores de acento (Normal, Verde, Coral,
  Naranja, Turquesa, Lila), panel reubicable, fondos (degradado, Lavanda, Tulipanes,
  color sólido, imagen propia) y el diseño **Liquid Glass** animado.
- Vive en la **bandeja del sistema**, puede **iniciar con Windows** y se ejecuta en
  segundo plano para que los avisos se disparen aunque la ventana esté cerrada.
- Interfaz en **español**. Datos 100 % locales (sin usuario ni login).

---

## Requisitos para compilar

- **Windows 10/11**.
- **Visual Studio 2022** (o *Build Tools*) con el *workload* "Desarrollo para
  escritorio con C++" (incluye el SDK de Windows y MSVC).
- **CMake 3.20+** (viene con Visual Studio).
- **vcpkg** (para `libcurl`, la única dependencia externa). ImGui, nlohmann/json y
  stb se descargan solos con CMake `FetchContent`.

## Compilar (paso a paso)

1. Instalar vcpkg (si no lo tienes):

   ```bat
   git clone https://github.com/microsoft/vcpkg C:\vcpkg
   C:\vcpkg\bootstrap-vcpkg.bat
   ```

2. Desde la carpeta del proyecto, configurar y compilar (vcpkg instalará `curl`
   automáticamente gracias a `vcpkg.json`):

   ```bat
   cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake
   cmake --build build --config Release
   ```

3. El ejecutable queda en `build\Release\AgendaPersonal.exe`. Las carpetas
   `assets\` y `shaders\` se copian automáticamente junto al `.exe`; deben viajar
   siempre con él (contienen las imágenes de fondo y los shaders del efecto).

> Si prefieres, abre la carpeta directamente en Visual Studio 2022
> ("Abrir una carpeta"): detecta el `CMakeLists.txt` y el `vcpkg.json` solo.

## Dónde se guardan los datos

Todo se guarda en un único JSON local:

```
%APPDATA%\Agenda Personal\agenda-personal-data.json
```

Contiene `tareas`, `config` y `ultimosAvisos` (control de qué avisos ya se
dispararon). Se puede respaldar o borrar sin afectar la instalación.

## Los shaders (efecto Liquid Glass)

Están en `shaders\` como archivos `.hlsl` legibles y **editables**: se compilan en
tiempo de ejecución, así que puedes ajustar el desenfoque, la refracción o el brillo
y volver a abrir la app para ver el cambio, sin recompilar el C++.

- `fullscreen.hlsl` — vértice/píxel para dibujar un quad a pantalla completa.
- `blur_kawase.hlsl` — desenfoque *Dual Kawase* (downsample + upsample).
- `glass.hlsl` — el material de vidrio: refracción por SDF de rectángulo redondeado,
  tinte translúcido, brillo de borde (rim) y reflejo diagonal en movimiento.

## Configurar avisos por correo

En **Configuración → Avisos por correo**: escribe tu correo y una **contraseña de
aplicación** (para Gmail se genera en la seguridad de tu cuenta de Google, con
verificación en dos pasos). SMTP por defecto: `smtp.gmail.com`, puerto `465`.

## Sincronización opcional

Ambas son opcionales; sin credenciales la app funciona igual con avisos locales y
correo.

- **Google Calendar**: crea credenciales OAuth (tipo *Aplicación de escritorio*) en
  Google Cloud Console, habilita la API de Calendar, pega *Client ID* y *Client
  Secret* en Configuración y pulsa **Conectar con Google**.
- **Apple Reminders / iCloud**: genera una **contraseña de aplicación** en
  appleid.apple.com, escribe tu *Apple ID* y esa contraseña, y activa iCloud.

---

## Nota

Este es un port nativo escrito para compilarse en Windows con MSVC. Si al compilar
por primera vez aparece algún error del compilador o del enlazador, es normal en un
proyecto nuevo de este tamaño: pásame el texto del error y lo ajustamos.
