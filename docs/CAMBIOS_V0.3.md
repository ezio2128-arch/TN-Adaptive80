# TN Adaptive 80 v0.3 — cambios derivados de pruebas reales

## Fallo confirmado de v0.2

- Resolución dinámica libre: microtirones y congelamientos de frame con DLSS + FG.
- Escala fija (`Min = Max = Emergency Min`): desaparecen los congelamientos y microtirones en Balanced, Performance y Extreme.
- Conclusión: el problema está ligado al churn de `renderSize`/historial temporal, no a falta de FPS por sí sola.

## Correcciones v0.3

1. Cambios de escala discretos.
2. Hold obligatorio tras cada cambio.
3. Recuperación lenta basada en estabilidad sostenida.
4. Un cambio de resolución no puede ocurrir frame a frame.
5. Probe de CPU espera a que el cambio se estabilice antes de evaluarlo.
6. CPU Guard restaura calidad por pasos retenidos.
7. Nuevo estado Mixed CPU + GPU.
8. Mixed nunca persigue Emergency Min por un cuello de botella parcialmente CPU.
9. Reset de FG solicitado solo en eventos reales de escala >= 0.03.
10. Presets Adaptive ya no cambian el qualityMode del upscaler.
11. Debug ampliado con escala solicitada, hold restante y tiempo estable en objetivo.

## Criterio de éxito

La prueba de Soledad debe alcanzar rendimiento similar a v0.2 sin congelamientos de un frame prolongados al caminar y mover la cámara. El objetivo sigue siendo aproximadamente 40 FPS reales / 80 con FG cuando la escena lo permita.
