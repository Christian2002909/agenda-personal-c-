; Script de Inno Setup para generar un instalador normal (no portable) de
; Agenda Personal: instala en Program Files, crea accesos directos en el
; Menu Inicio (y opcionalmente en el Escritorio) y registra un desinstalador
; en "Agregar o quitar programas".
;
; Requiere tener compilada la app en Release antes de correr este script:
;   cmake --build build --config Release
;
; Como generar el instalador:
;   1. Instalar Inno Setup (gratis): https://jrsoftware.org/isinfo.php
;   2. Abrir este archivo con el "Inno Setup Compiler" y pulsar "Compile"
;      (o por linea de comandos: "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" installer\AgendaPersonal.iss)
;   3. El instalador queda en installer\output\AgendaPersonal-Setup.exe

#define MyAppName "Agenda Personal"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "Christian"
#define MyAppExeName "AgendaPersonal.exe"

[Setup]
AppId={{B3B7B6C0-6E1C-4E9B-9B1A-6C7A9B0D8F21}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
UninstallDisplayIcon={app}\{#MyAppExeName}
OutputDir=output
OutputBaseFilename=AgendaPersonal-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
; Instala para el usuario actual, sin pedir permisos de administrador.
PrivilegesRequired=lowest
ArchitecturesInstallIn64BitMode=x64compatible
DisableProgramGroupPage=yes

[Languages]
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"

[Tasks]
Name: "desktopicon"; Description: "Crear un acceso directo en el Escritorio"; GroupDescription: "Accesos directos adicionales:"
Name: "autostart"; Description: "Iniciar Agenda Personal junto con Windows"; GroupDescription: "Opciones:"; Flags: unchecked

[Files]
Source: "..\build\Release\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\assets\*"; DestDir: "{app}\assets"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\shaders\*"; DestDir: "{app}\shaders"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\Desinstalar {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Registry]
; "Iniciar con Windows": mismo mecanismo (HKCU Run) que usa la app desde Configuracion,
; asi que si el usuario despues lo cambia desde ahi, sigue funcionando igual.
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "Agenda Personal"; ValueData: """{app}\{#MyAppExeName}"""; Tasks: autostart

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Abrir {#MyAppName}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
; Por si la app dejo el shader/log temporal; no borra los datos del usuario
; (%APPDATA%\Agenda Personal\agenda-personal-data.json) para no perder sus tareas.
Type: filesandordirs; Name: "{app}\assets"
Type: filesandordirs; Name: "{app}\shaders"
