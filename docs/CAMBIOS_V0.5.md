# Cambios v0.5

1. Corrige clasificación errónea observada en v0.4: casos como pre-FG ~44 ms / GPU ~18 ms ya no deben marcarse como GPU-bound.
2. Añade comprobación de alineación entre timestamp GPU retrasado y frametime actual.
3. Añade `GPU Pressure Qualification`: no hay cambio de resolución por un pico corto de cámara/streaming.
4. Balanced/Performance/Extreme usan Hold más largo: 500/420/380 ms.
5. 40 FPS es el mínimo deseable. Se añade reserva de +8/+10/+12 FPS antes de recuperar calidad.
6. El rango seguro DLSS de v0.4 se conserva.
7. Muestreo GPU reducido para bajar el coste propio de AD80.
8. Motion Blur permanece separado y no se empaqueta con AD80.
