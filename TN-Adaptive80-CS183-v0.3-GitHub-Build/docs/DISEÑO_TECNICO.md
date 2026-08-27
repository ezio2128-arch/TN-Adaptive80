# Diseño técnico — TN Adaptive 80 v0.3 Stable Dynamic Resolution

El controlador sigue leyendo frametime pre-FG y timestamps GPU, pero separa la decisión lógica de la aplicación real de resolución.

## Pipeline de decisión

1. Suavizado asimétrico de frametime.
2. Clasificación GPU / Mixed / CPU-Engine.
3. Cálculo de `requestedScale`.
4. Cuantización por `Resolution Step`.
5. Aplicación solo si `Resolution Hold` ha expirado.
6. Recolección de nueva evidencia antes de otro cambio.
7. Recuperación solo tras `Target Hold` y la cadencia impuesta por `Recovery Speed`.

## FG History Guard

`FidelityFX::RequestFrameGenerationHistoryReset()` marca un único reset pendiente cuando un evento de escala aplicado supera 0.03. `Present()` combina ese evento con los resets HDR existentes. La petición se consume una sola vez y Resolution Hold evita una cadena continua de resets.

## Mixed Bound

Si el frametime total está fuera del objetivo y el GPU frametime también supera el presupuesto GPU, pero la GPU no explica la mayoría del frame total, se clasifica como Mixed. En Mixed la resolución puede bajar de forma moderada hasta `minScale`, pero no persigue `emergencyMinScale`.
