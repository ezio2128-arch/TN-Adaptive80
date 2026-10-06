# Continuación exacta en un entorno Windows

La carpeta contiene código fuente y dependencia PresentMon, no una release.
No rehacer el diseño ni dar por compilados los proyectos.

1. Abrir FramePulse.sln con VS 2022, UWP/.NET Native y SDK 26100.
2. Ejecutar Installer/Build.ps1. Resolver errores de MSVC/WinRT/XAML/manifest.
3. Capturar salida de FramePulseTests.exe; deben pasar 32 comprobaciones.
4. Validar ruta fullTrustProcess, bridge mismo paquete, versiones/frameworks.
5. Firmar localmente e instalar. Confirmar Win+G y ciclo de vida.
6. Usar FramePulseTestGame y attachment explícito, FG apagado. Medir Manual60.
7. Probar 120→80 y recuperación con Adaptive; exportar logs y capturas por frame.
8. Probar Skyrim moddeado sin FG/limiters extra, después agregar cambios de uno en uno.
9. Hacer los benchmarks A/B; registrar datos reales, sin rellenar tablas con estimados.
10. Completar ACCEPTANCE.md antes de cualquier release.

Bugs de runtime más probables a revisar: schema de registro startup/fullTrust,
bootstrap SDK de Game Bar, ciclo App Services cuando se cierra el último widget,
permisos ETW y formato de columnas CSV 2.6.0, swapchain principal y direcciones del
hook bajo proxies/overlays. Cambiar arquitectura sólo si las pruebas lo justifican.

Restricciones que deben permanecer: nunca cap externo ficticio, nunca asumir x2 FG,
nunca identificar tearing support como G-Sync activo, nunca bypass anti-cheat,
no servicios/admin automáticos, no modificaciones permanentes del juego.
