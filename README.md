# TN Adaptive 80 v0.4 — DLSS Safe Dynamic Resolution

Build bundle para **Community Shaders 1.8.3 / True North**. v0.4 ataca los congelamientos observados en v0.2-v0.3 al dejar de usar escalas DLSS arbitrarias.

## Cambios clave
- Consulta `slDLSSGetOptimalSettings` y usa `renderWidthMin/Max` + `renderHeightMin/Max`.
- AD80 nunca solicita una resolución DLSS fuera del rango informado por NVIDIA.
- Si DLSS no informa un rango dinámico válido, AD80 bloquea la escala al tamaño óptimo/seguro en vez de adivinar.
- Mantiene Resolution Hold, Target Hold, CPU Guard, Mixed Guard y Fast Attack/Slow Recovery.
- Elimina los resets experimentales de Frame Generation añadidos en v0.3.
- **TN Smooth Motion Blur ya NO está incluido.** Debe instalarse como mod separado cuando quieras probarlo.
- Debug muestra escala aplicada, resolución interna, rango efectivo AD80, rango seguro DLSS y tamaño óptimo DLSS.

## Compilación
Usa `.github/workflows/build-mo2.yml` o copia `WORKFLOW-build-mo2-v0.4.yml` sobre el workflow de tu repositorio. El artifact final debe llamarse `TN-Adaptive80-CS183-v0.4-MO2`.

## Orden MO2
1. Community Shaders 1.8.3
2. Upscaling
3. Community Shaders True North Settings
4. TN Adaptive 80 v0.4
5. TN Smooth Motion Blur (opcional, separado)

Primera prueba recomendada: Soledad, AD80 Balanced, FG OFF; luego FG ON.
