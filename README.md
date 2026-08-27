# TN Adaptive 80 — Community Shaders 1.8.3 — build automático para MO2

Este repositorio pequeño **no sustituye el proyecto original**. Contiene el overlay TN Adaptive 80 v0.2 ya validado y una acción de GitHub que construye el binario Windows faltante sobre el tag exacto `v1.8.3` de Community Shaders con sus submódulos recursivos.

## Qué genera

Al terminar correctamente la acción `Build TN Adaptive 80 MO2`, el artefacto es:

`TN-Adaptive80-CS183-v0.2-MO2.zip`

Su raíz es directamente instalable con Mod Organizer 2 y contiene:

- `SKSE/Plugins/CommunityShaders.dll` compilado con Adaptive 80.
- Traducciones `en.json` y `es.json` con el menú Adaptive 80.
- `Shaders/ISTemporalAA.hlsl` con TN Smooth Motion Blur Alpha 0.4 Adaptive 10 FPS Rescue.
- Documentación técnica.
- `meta.ini` para MO2.

## Orden de instalación previsto en MO2

1. Community Shaders 1.8.3.
2. Upscaling 1.4.0 / el Upscaling actual de True North.
3. Community Shaders True North Settings.
4. **TN Adaptive 80 CS183 v0.2**.

Desactiva el mod TN Smooth Motion Blur independiente cuando actives este paquete, porque Adaptive 80 ya incorpora el mismo `ISTemporalAA.hlsl` recomendado.

## Lo que NO se incluye

No se incluyen `SettingsUser.json`, `SettingsDefault.json` ni `CommunityShaders_ImGui.ini`; de esta forma los ajustes True North existentes no son reemplazados.

## Prueba inicial recomendada

Usa primero el preset **Adaptive 80 Balanced**. El objetivo predeterminado es 40 FPS reales/pre-FG y aproximadamente 80 FPS de salida con FG activo. Verifica primero una partida de prueba antes de convertirlo en parte permanente de la lista.
