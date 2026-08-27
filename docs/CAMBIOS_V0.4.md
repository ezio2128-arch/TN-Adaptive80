# Cambios v0.4

- DLSS Safe Dynamic Resolution mediante `slDLSSGetOptimalSettings`.
- Clamp por dimensiones reales de entrada, alineadas a 8 dentro del rango permitido.
- Fallback seguro a escala fija si el rango DLSS no está disponible.
- Sin modificaciones experimentales de FidelityFX/FG.
- Motion Blur separado completamente de AD80.
- Debug ampliado para validar límites y resolución interna.
