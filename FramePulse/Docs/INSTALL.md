# Compilación e instalación en Windows 11 x64

**No hay instalador terminado incluido.** Estos pasos corresponden al candidato
fuente. La compilación y la instalación Windows todavía no se han ejecutado.

## Herramientas

- Visual Studio 2022, MSVC v143 C++ x64.
- Herramientas C# UWP, XAML, compilación .NET Native y Windows SDK 10.0.26100.0.
- NuGet habilitado para Microsoft.Gaming.XboxGameBar 7.2.240903001 y
  Microsoft.NETCore.UniversalWindowsPlatform 6.2.9.
- Windows 11 y Xbox Game Bar instalado/habilitado.

El nombre visible del workload UWP cambia entre versiones del instalador de
Visual Studio. Comprueba los componentes UWP y .NET Native; instalar solamente
WinUI/Windows App SDK no proporciona este pipeline UWP.

## Construir

Extrae el ZIP, abre PowerShell en la carpeta FramePulse y ejecuta:

```powershell
.\Installer\Build.ps1
```

Se usa FramePulse.sln, se ejecutan los tests y se prepara
`out\FramePulse_1.0.0_candidate.msix`. Sin certificado será **sin firmar** y
no se instalará con el flujo normal. Build aborta si falla cualquier compilación,
prueba o hash de PresentMon. El native-only alternativo es CMake; no construye UWP.

## Firmar

Usa un certificado de firma de código cuyo Subject coincida exactamente con
`CN=FramePulse Development`, o cambia Publisher en el manifest para tu certificado.
La clave privada debe estar bajo tu control. No hay PFX ni clave privada en este
proyecto. Para desarrollo puedes crear un certificado local:

```powershell
$cert = New-SelfSignedCertificate -Type Custom -Subject 'CN=FramePulse Development' -KeyUsage DigitalSignature -FriendlyName 'FramePulse development' -CertStoreLocation 'Cert:\CurrentUser\My' -TextExtension @('2.5.29.37={text}1.3.6.1.5.5.7.3.3','2.5.29.19={text}')
.\Installer\Build.ps1 -CertificateThumbprint $cert.Thumbprint
Export-Certificate -Cert $cert -FilePath .\out\FramePulse-development.cer
```

Para sideload, el equipo de pruebas debe confiar en el certificado. Importa sólo
ese certificado de desarrollo, conscientemente, en **Local Machine / Trusted
People**, según la guía Microsoft; esto requiere derechos administrativos.
No añadas certificados arbitrarios a Trusted Root ni deshabilites verificación
de firmas. Para distribución pública usa firma apropiada/Store.

## Instalar

Las dependencias UWP se generan junto al paquete en `out\packages`. Si no están
instaladas, pásalas con DependencyPath:

```powershell
.\Installer\Install.ps1 -Package .\out\FramePulse_1.0.0_candidate.msix
# Si Windows informa frameworks ausentes:
# .\Installer\Install.ps1 -Package <msix> -DependencyPath <rutas a frameworks x64 generados>
```

Abre FramePulse desde Inicio una vez. Después Win+G → Widgets → FramePulse.
Prueba pinning y transparencia en los controles de Game Bar.

## Primer test controlado

Inicia FramePulseTestGame.exe incluido en la carpeta Core del paquete compilado.
El programa DX11 admite teclas 1 (capacidad 120), 2 (80), 3 (200). No pretende
simular una GPU real: verifica el camino de pacing y recuperación.

En el widget confirma FG off, selecciona Advanced Pacing y pulsa **Attach DXGI
pacing to this game**. Comprueba PACING ACTIVE, luego Manual 60 y Adaptive.
Si no aparece activo, no supongas que el cap se está aplicando.

## Skyrim

Primera prueba: sesión offline, FG desactivado, sin otro limiter. Identifica
SSE Display Tweaks, cap del driver, RTSS y VSync antes de comparar. FramePulse no
lee de forma universal esos ajustes; RTSS sólo produce una advertencia indicativa.

La misma acción de attachment carga el DLL en memoria. No se escriben DLLs,
INIs ni plugins en la carpeta del juego. Safe Mode deja la integración dormida.
Cierra el juego para retirar por completo el DLL. Una prueba con tu modlist sigue
siendo necesaria antes de declarar compatibilidad.

La opción Start Core with Windows usa StartupTask, desactivado por defecto. El
registro empaquetado y su comportamiento también necesitan prueba Windows.

## ETW

Algunos equipos requieren pertenecer a Performance Log Users o permisos para
iniciar sesiones ETW. El proyecto no eleva el juego ni el Core automáticamente.
Si PresentMon termina sin muestras, investiga el error y permisos antes de activar
Adaptive. No se instala un servicio privilegiado como solución automática.
