# Validación TN Adaptive 80 v0.3

## Validación portable realizada

El `AdaptiveResolutionController` se compila y ejecuta de forma aislada con C++20. Las pruebas cubren:

- estabilidad dentro de la banda objetivo;
- GPU-bound con cambios retenidos, nunca frame-a-frame;
- CPU-bound conservando calidad;
- Mixed bound con GPU también por encima del presupuesto;
- modo de escala fija sin ningún evento de resize;
- Rescue/Emergency mediante cambios retenidos;
- recuperación de calidad lenta.

El workflow vuelve a ejecutar estas pruebas con MSVC antes de compilar Community Shaders.

## Validación que requiere Skyrim

- ausencia de freezes en Soledad con Balanced/Performance/Extreme;
- comportamiento de FG History Guard;
- estabilidad visual de DLSS al cambiar render size;
- funcionamiento del nuevo estado Mixed;
- ausencia de regresiones en HUD y TN Smooth Motion Blur.
