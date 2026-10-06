# FramePulse — candidato de desarrollo 1.0.0

**Estado: NO ES UNA RELEASE 1.0.0 TERMINADA NI UN INSTALADOR VALIDADO.**

Este archivo contiene implementación real en C++20 y C# UWP, no un mockup.
El entorno de desarrollo disponible fue Linux, sin MSVC, Windows SDK, Xbox Game
Bar ni acceso al equipo Acer Nitro. Por eso sólo se compiló y ejecutó el código
portable. No se incluyen FramePulseCore.exe, FramePulsePacing.dll compilados ni
un MSIX firmado. PresentMon.exe es una dependencia oficial, NO el Core.

## Contenido implementado

- Motor adaptativo con medianas, P95/P99, 1% lows, histéresis y sondeos de recuperación.
- Core Win32 C++: detección por eventos, perfiles JSON, monitor/refresco e IPC App Services.
- Adaptador externo de PresentMon 2.6.0 mediante stdout, sin servicio propio.
- Limitador DXGI opcional: QPC, timer de alta resolución y spin final de hasta 50 µs.
- Hook Present/Present1 y attachment explícito, experimental, limitado a SkyrimSE y TestGame.
- Widget UWP XAML: gráfico, Manual/Adaptive, transparencia/click-through de Game Bar.
- Política conservadora de bloqueo, Safe Mode y detección indicativa de componentes FG.
- Solución Visual Studio, proyectos CMake, scripts MSIX y desinstalación.
- Icono vectorial original, PNG 16/32/64/128/256 y archivo ICO.
- Harness DX11, pruebas portables y utilidades de medición.

## Lo esencial

**Safe Mode mide; NO aplica caps.** El pacing de un juego externo no puede
implementarse durmiendo el proceso del Core. Advanced Pacing ejecuta la espera
junto a Present dentro del juego. La instalación nunca realiza ese attachment.

En juegos no cubiertos, el target adaptativo es una recomendación. La interfaz
sólo muestra PACING ACTIVE cuando el hook confirma actividad reciente. No se
promete DX12, Vulkan, OpenGL ni FG controlados por el limitador actual.

## Primer paso en Windows

Lee [Docs/INSTALL.md](Docs/INSTALL.md), abre `FramePulse.sln`, compila con
`Installer/Build.ps1`, firma el paquete e instala. La compilación Windows aún
puede revelar errores que necesitan corregirse. Completa
[Docs/ACCEPTANCE.md](Docs/ACCEPTANCE.md) antes de llamar release al resultado.

## Prueba portable reproducible

```sh
g++ -std=c++20 -O2 -Wall -Wextra -Werror -Wpedantic -I . Tests/engine_tests.cpp -o FramePulseTests
./FramePulseTests
python3 Tests/validate_sources.py
```

El Python de las herramientas sólo es opcional para desarrollo/análisis offline;
FramePulse no integra ni distribuye un runtime Python.

Más detalles: [arquitectura](Docs/ARCHITECTURE.md), [investigación](Docs/RESEARCH.md),
[limitaciones](Docs/LIMITATIONS.md), [rendimiento](Docs/BENCHMARKS.md),
[compatibilidad](Docs/COMPATIBILITY.md), [problemas](Docs/TROUBLESHOOTING.md).
