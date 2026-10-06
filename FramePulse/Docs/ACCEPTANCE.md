# Puerta de aceptación 1.0.0

**Release status: BLOCKED.** Ninguno de los tests Windows siguientes fue ejecutado.
El número 1.0.0 en manifest/proyectos reserva la versión candidata; no certifica
que el producto cumpla los criterios originales.

| Criterio del usuario | Estado actual | Evidencia necesaria |
|---|---|---|
| Instalar / reiniciar | Pendiente | MSIX firmado aceptado por Windows |
| Win+G → FramePulse | Pendiente | Captura de registro/activación del widget |
| FPS/frametime reales y gráfica | Pendiente | Comparación PresentMon con juego real |
| Manual modifica cap | Pendiente | Captura antes/después, hook activo |
| Adaptive modifica target y cap | Target validado sintéticamente; cap pendiente | Harness y Skyrim |
| Cerrar Game Bar mantiene motor | Código diseñado; pendiente | Sesión sin widget y misma limitación |
| Cerrar juego vuelve a idle | Código diseñado; pendiente | Proceso/captura terminados, CPU/RAM |
| Guardar perfiles | Código Windows; pendiente | Reabrir juego y verificar JSON/config |
| Widget aparece nuevamente | Pendiente | Aperturas repetidas y reinicio |
| Idle sin coste significativo | Pendiente | Medición prolongada, widget abierto/cerrado |
| Desinstalación completa | Script preparado; pendiente | Paquete/startup/core retirados; perfiles opcionales |
| Anti-cheat bloquea pacing | Reglas y guards; pendiente | Casos positivos/negativos sin evadir sistemas |
| Overhead aceptable | Pendiente | A/B en Acer Nitro y segundo equipo |

No renombrar el archivo de candidato a `FramePulse_1.0.0.msix` como mecanismo para
aprobarlo. Guardar logs, versión SDK, driver, Game Bar, método de telemetría,
configuración del juego y resultado de cada prueba. No marcar checks por inspección
del código. Hasta entonces la entrega es un candidato fuente parcialmente verificado.
