# Compatibilidad con los adjuntos True North

## Base elegida

Se usó `skyrim-community-shaders-1.8.3(2).zip` como base exacta. El árbol `skyrim-community-shaders-dev(2).zip` se comparó, pero no se mezclaron sus cambios posteriores: la entrega queda anclada a 1.8.3 como se solicitó.

## Addon Upscaling

`Upscaling(2).zip` corresponde al addon Nexus Upscaling 1.4.0 y contiene sus shaders y binarios de DLSS/Streamline/FidelityFX. Algunos shaders difieren del árbol base, así que el overlay Adaptive 80 no incluye copias de esas rutas que los puedan sobrescribir.

Orden futuro previsto en MO2:

1. Community Shaders 1.8.3.
2. Upscaling 1.4.0.
3. Ajustes True North.
4. TN Adaptive 80 compilado, que ya incorporará TN Smooth Motion Blur Alpha 0.4.

La versión anterior de TN Smooth Motion Blur deberá desactivarse para evitar que otro mod vuelva a sobrescribir `Shaders/ISTemporalAA.hlsl`. El orden definitivo deberá verificarse en un perfil de prueba de MO2 después de compilar la DLL.

## Ajustes True North detectados

La configuración aportada usa:

| Ajuste | Valor conservado |
|---|---:|
| Método principal | DLSS |
| Método sin DLSS | FSR |
| Calidad | Balanced |
| Preset DLSS | M |
| Frame Generation | Activado |
| Frame limiter | Activado |
| FG en menús | Activado |
| Sharpening DLSS | Activado, 0.20 |

No se reemplazan `SettingsUser.json`, `SettingsDefault.json` ni `CommunityShaders_ImGui.ini`. El cargador JSON de Community Shaders mantiene estas claves y añade los valores Adaptive 80 que faltan mediante sus defaults.

## TN Smooth Motion Blur

Se auditó `TN_SmoothMotionBlur_CS183_Alpha04_Adaptive10FPS(1).zip`:

- `Reference/ISTemporalAA-CS183-original.hlsl` coincide byte por byte con el shader estándar de CS 1.8.3.
- `Shaders/ISTemporalAA.hlsl` coincide byte por byte con el preset opcional `Adaptive 10FPS Rescue`.
- No se eligió `Maximum 10FPS Rescue`, porque es más agresivo y presenta mayor riesgo de borrón visible.
- El shader integrado usa los motion vectors ya enlazados por el pase temporal; no añade optical flow, texturas ni compute passes.
- El HUD/UI se compone fuera de este pase temporal según la arquitectura existente.
