# Validación

## Completada

- Compilación portable del controlador con GCC C++23, `-O2 -Wall -Wextra -Werror`.
- Prueba de la banda estable de 22–27 ms sin oscilación.
- Prueba de recuperación de una carga GPU-bound de 35 ms.
- Prueba de CPU-bound a 50 ms conservando calidad.
- Prueba de fallback cuando no hay timestamps GPU.
- Prueba de timestamp GPU engañosamente alto sin colapso de resolución.
- Prueba de Emergency con recuperación lenta posterior.
- Validación JSON de las traducciones inglesa y española.
- `extract-i18n.py --check`: catálogo inglés actualizado.
- `extract-i18n.py --orphans`: ninguna clave huérfana.
- `sort-i18n.py --check`: todos los catálogos ordenados.
- Comprobación de whitespace sobre cada archivo cambiado.
- Comparación de rutas del addon Upscaling y de los ajustes True North.
- Integridad completa del ZIP TN Smooth Motion Blur Alpha 0.4.
- Coincidencia SHA-256 entre su referencia y el shader estándar de CS 1.8.3.
- Coincidencia SHA-256 entre el preset principal y `Adaptive 10FPS Rescue`.
- Preprocesado estático de `ISTemporalAA.hlsl` para las variantes SDR y HDR.

## Pendiente y bloqueante para una versión instalable

- Compilar `CommunityShaders.dll` Release con MSVC/Windows SDK.
- Resolver los submódulos exactos `CommonLibSSE-NG`, `Streamline-DX12` y `FidelityFX-SDK`.
- Ejecutar el build universal `ALL-VS2022` sin warnings.
- Probar arranque en Skyrim SE/AE mediante SKSE.
- Validar DLSS, FSR fallback y Frame Generation en un perfil limpio de MO2.
- Medir pacing, latencia, consumo y VRAM en la RTX 4060 Laptop de 75 W.
- Verificar el HUD con TrueHUD, Wheeler, subtítulos, diálogos e inventario.
- Compilar `ISTemporalAA.hlsl` con el compilador HLSL de la toolchain Windows y cero warnings.
- Comprobar visualmente TN Smooth Motion Blur Alpha 0.4 a 40, 25, 15 y 10 FPS reales.
- Confirmar que el preset Balanced se comporta bien en varias escenas de True North; los valores incluidos son un punto de partida técnico, no una medición física ya realizada.
