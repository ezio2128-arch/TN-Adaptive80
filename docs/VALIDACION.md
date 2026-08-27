# Validación v0.5

Pruebas portables incluidas:
- 40 FPS tratado como suelo, no techo.
- recuperación de calidad solo con rendimiento sobrante real.
- caso observado del usuario pre-FG 44 ms / GPU 18 ms clasificado como CPU/no-GPU y sin bajar escala.
- timestamp GPU retrasado durante giro de cámara no promueve GPU-bound.
- un pico GPU corto (< qualification) no cambia resolución.
- presión GPU sostenida sí permite cambios discretos.
- Mixed Guard no persigue escala de emergencia.
- escala fija no genera churn.
- clamp de rango seguro del proveedor se conserva.

Antes de instalar el artifact final, confirmar que GitHub Actions termina verde y que el artifact se llama `TN-Adaptive80-CS183-v0.5-MO2`.
