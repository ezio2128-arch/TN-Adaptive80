# TN Adaptive 80 v0.5 — Performance Reserve + Transient Guard

Build bundle para **Community Shaders 1.8.3 / True North**. Esta versión parte de los resultados reales de v0.4: el rango seguro de DLSS funcionó y eliminó las escalas peligrosas, pero todavía quedaron microtirones al girar la cámara y una clasificación GPU/CPU incorrecta en algunos picos.

## Qué cambia

- **40 FPS reales pasan a ser un suelo deseable, no un techo.** Cada preset mantiene una reserva de rendimiento antes de gastar FPS extra recuperando resolución.
- **Clasificador GPU / Mixto / CPU corregido** usando la contribución GPU respecto al frametime pre-FG actual.
- **Transient Guard:** un giro de cámara/streaming no puede provocar un resize inmediato con un timestamp GPU retrasado.
- **GPU Pressure Qualification:** la carga GPU debe mantenerse durante varios cientos de ms antes de cambiar la resolución.
- **Resolution Hold más largo** para reducir transiciones visibles.
- **Menor coste de diagnóstico GPU:** timestamps D3D11 cada 2 frames normalmente y cada 4 frames durante CPU Guard.
- Se conserva el **DLSS Safe Range** de v0.4 y ningún preset puede salir de los límites reportados por NVIDIA.
- **TN Smooth Motion Blur sigue totalmente separado.** Este bundle no contiene `ISTemporalAA.hlsl`.

## Presets iniciales v0.5

- Balanced: mínimo 40 FPS, reserva +8 FPS, Hold 500 ms, confirmación GPU 450 ms.
- Performance: mínimo 40 FPS, reserva +10 FPS, Hold 420 ms, confirmación GPU 350 ms.
- Extreme Rescue: mínimo 40 FPS, reserva +12 FPS, Hold 380 ms, confirmación GPU 300 ms.

Los mínimos de resolución siguen sujetos al rango seguro que reporte DLSS. En la RTX 4060/1080p de las pruebas v0.4, DLSS reportó aproximadamente `0.500–1.000`.

## Compilación

Para un repositorio nuevo puede usarse `.github/workflows/build-mo2.yml`.

Para el repositorio actual del proyecto, donde las versiones quedan como carpetas anidadas, copia el contenido de:

`WORKFLOW-build-mo2-v0.5-NESTED.yml`

sobre `.github/workflows/build-mo2.yml`.

El artifact esperado es:

`TN-Adaptive80-CS183-v0.5-MO2`

## Orden MO2

1. Community Shaders 1.8.3
2. Upscaling
3. Community Shaders True North Settings
4. TN Adaptive 80 v0.5
5. TN Smooth Motion Blur (opcional, separado, solo cuando quieras probarlo)

v0.5 es una **versión de prueba**. Primero validar Soledad/Balanced con y sin FG antes de considerarla estable.
