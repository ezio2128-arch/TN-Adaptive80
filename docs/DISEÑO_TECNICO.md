# Diseño técnico

## Objetivo efectivo

El objetivo nativo se calcula en cada frame como:

```text
min(Target Native FPS, Target Output FPS / multiplicador FG)
```

Con los valores predeterminados y FG activo: `min(40, 80/2) = 40 FPS`, equivalentes a 25 ms pre-FG. Si FG no está activo, el controlador no finge duplicación alguna.

## Zonas de control

Con el objetivo predeterminado de 25 ms:

| Frametime pre-FG | Estado | Acción |
|---:|---|---|
| ≤22 ms | Quality | Recuperar calidad lentamente |
| 22–27 ms | Target | Mantener la escala |
| >27–40 ms | Rescue | Reducir carga de forma progresiva |
| >40–65 ms | Rescue | Aplicar ataque reforzado |
| >65 ms | Emergency | Permitir el suelo de emergencia |

El frametime usa un filtro asimétrico: reacciona con rapidez cuando empeora y con más lentitud cuando mejora. La banda muerta evita alternancias continuas entre dos escalas.

## Escala y resolución

La resolución interna se alinea a múltiplos de ocho antes de alimentar DLSS/FSR. El render final conserva el tamaño de salida del juego. A 1920×1080, los límites aproximados son:

| Preset | Máxima | Mínima | Emergencia | Base DLSS |
|---|---:|---:|---:|---|
| Balanced | 0.70 | 0.52 | 0.44 | Balanced |
| Performance | 0.64 | 0.48 | 0.42 | Performance |
| Extreme Rescue | 0.60 | 0.44 | 0.38 | Performance |

Las escalas son ajustables. No se promete alcanzar 40 FPS reales cuando el coste no depende de la resolución o cuando el suelo configurado no es suficiente.

## CPU Guard

El controlador crea un anillo de cuatro muestras D3D11. Cada muestra usa tres queries —disjoint, inicio y fin— y se consulta con `D3D11_ASYNC_GETDATA_DONOTFLUSH`, por lo que no fuerza una espera de CPU por resultados de GPU.

El diagnóstico combina:

1. Relación entre tiempo GPU medido y frametime de referencia.
2. Confirmación durante varias muestras para evitar cambios por ruido.
3. Una reducción de prueba pequeña cuando el origen aún es dudoso.
4. Comprobación de si esa reducción produjo una mejora medible.

Si la carga apenas responde, CPU Guard restaura la escala previa y la mantiene temporalmente. También protege frente a timestamps engañosamente altos: una lectura de GPU por sí sola no invalida la prueba de respuesta. El resultado sigue siendo una heurística y el menú lo etiqueta como estimación.

## Ataque y recuperación

- La bajada máxima depende de segundos, no de número de frames.
- Rescue multiplica el ataque; Emergency lo multiplica todavía más.
- La recuperación predeterminada es `0.04` unidades de escala por segundo.
- Cuando CPU Guard corrige una bajada inútil, la restauración es más rápida que la recuperación ordinaria.
- Una prueba activa detiene reducciones adicionales hasta observar su efecto, evitando colapsar la imagen durante una caída de CPU.

## Coste propio

No se añade ningún compute pass ni textura de análisis. El coste de CPU es aritmética escalar y unas pocas ramas. El único estado GPU nuevo son 12 objetos query pequeños reutilizados en anillo. Las consultas son no bloqueantes.

## HUD, motion blur y Frame Generation

Adaptive 80 cambia las proporciones de resolución dinámica ya usadas por el addon de Upscaling. El `ISTemporalAA.hlsl` integrado conserva la base exacta de CS 1.8.3 y añade TN Smooth Motion Blur Alpha 0.4 después del resultado temporal de escena y antes de la conversión HDR de salida.

El motion blur usa el `velocityTex` ya enlazado. Los píxeles casi estáticos salen sin taps adicionales; el movimiento ordinario usa tres muestras direccionales y la rama de Rescue añade dos más cuando el desplazamiento por frame supera el umbral extremo. Un rechazo barato por diferencia de color reduce el sangrado entre bordes. No se añaden motion estimation, optical flow, texturas ni buffers.

La ruta existente vuelve a escala completa después del reescalado y antes de la composición posterior/UI. El pase temporal es de escena, por lo que no introduce blur ni reducción propia en brújula, TrueHUD, subtítulos o menús.

Frame Generation sigue siendo el de Community Shaders: no se implementa Multi Frame Generation. Las estadísticas de salida se muestran como una estimación de `FPS nativos × 2` solo cuando la ruta FG está activa.
