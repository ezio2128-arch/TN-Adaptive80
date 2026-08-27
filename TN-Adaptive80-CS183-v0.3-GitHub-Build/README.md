# TN Adaptive 80 v0.3 — Stable Dynamic Resolution — Community Shaders 1.8.3

Este repositorio contiene el overlay de **TN Adaptive 80 v0.3** y el workflow de GitHub Actions que compila `CommunityShaders.dll` sobre el tag exacto `v1.8.3` de Community Shaders y genera un ZIP instalable con Mod Organizer 2.

## Por qué existe v0.3

Las pruebas reales de v0.2 en True North aislaron un problema reproducible: con resolución dinámica libre, DLSS + Frame Generation podía presentar microtirones y congelamientos de un solo frame; al fijar `Min = Max = Emergency Min` los tres perfiles dejaban de congelarse. v0.3 conserva el controlador y CPU Guard que sí funcionaban, pero cambia la forma de aplicar la resolución.

## Cambios principales v0.3

- **Stable Dynamic Resolution:** la escala ya no cambia de forma continua cada frame.
- **Resolution Step:** cambios discretos (por defecto 0.04).
- **Resolution Hold:** tiempo de estabilización tras cada cambio aplicado.
- **Target Hold / Slow Recovery:** recuperación de calidad solo después de rendimiento estable.
- **Requested Scale vs Applied Scale:** el debug muestra lo que quiere el controlador y lo que realmente está aplicado.
- **Mixed Bound:** distingue `GPU`, `Mixed CPU + GPU` y `CPU / Engine`.
- **Mixed Guard:** descarga moderadamente la GPU sin perseguir el mínimo de emergencia cuando el motor/CPU también limita.
- **CPU Guard discreto:** también restaura calidad mediante pasos retenidos; ya no modifica la escala cada frame.
- **FG History Guard:** un cambio real y retenido de escala puede pedir un único reset de historial de FidelityFX FG; nunca se resetea continuamente.
- **Presets desacoplados del modo interno de DLSS/FSR:** cambiar Balanced/Performance/Extreme de Adaptive 80 ya no cambia automáticamente `qualityMode`.
- Se conserva **TN Smooth Motion Blur Alpha 0.4 Adaptive 10 FPS Rescue** sin cambios funcionales.
- No se incluyen ni sobrescriben los settings específicos de True North.

## Presets v0.3

### Balanced
- Min 0.52 / Max 0.70 / Emergency 0.44
- Fast Attack 0.80
- Recovery 0.040
- GPU Headroom 0.90
- Resolution Step 0.04
- Resolution Hold 280 ms
- Target Hold 800 ms

### Performance
- Min 0.48 / Max 0.64 / Emergency 0.42
- Fast Attack 1.05
- Recovery 0.030
- GPU Headroom 0.88
- Resolution Step 0.04
- Resolution Hold 240 ms
- Target Hold 700 ms

### Extreme Rescue
- Min 0.44 / Max 0.60 / Emergency 0.38
- Fast Attack 1.35
- Recovery 0.025
- GPU Headroom 0.86
- Resolution Step 0.04
- Resolution Hold 220 ms
- Target Hold 650 ms

## Qué genera GitHub Actions

`TN-Adaptive80-CS183-v0.3-MO2.zip`

Contenido principal:

- `SKSE/Plugins/CommunityShaders.dll`
- `SKSE/Plugins/CommunityShaders/Translations/en.json`
- `SKSE/Plugins/CommunityShaders/Translations/es.json`
- `Shaders/ISTemporalAA.hlsl`
- documentación
- `meta.ini`

## Orden de instalación MO2

1. Community Shaders 1.8.3
2. Upscaling 1.4.0 / Upscaling actual de True North
3. Community Shaders True North Settings
4. **TN Adaptive 80 v0.3**

Desactiva la instalación independiente de TN Smooth Motion Blur Alpha 0.4 cuando uses este paquete, porque v0.3 ya lo incluye.

## Primera prueba recomendada

1. Portátil conectado y ventilación normal.
2. Adaptive 80 **Balanced**.
3. Debug Statistics activado.
4. Repetir la ruta de Soledad donde v0.2 se congelaba.
5. Caminar, correr y girar la cámara de forma continua durante 2–3 minutos.
6. Verificar que `Resolution Hold` impide cambios consecutivos y que no aparecen congelamientos largos.
7. Luego repetir con Performance y Extreme.
8. Finalmente repetir una escena mixta/CPU como la prueba de los dos dragones.

v0.3 sigue siendo una versión de prueba técnica: el objetivo inmediato es **estabilidad de la resolución dinámica**, no perseguir más FPS que los ~40 reales / ~80 FG ya observados en v0.2.
