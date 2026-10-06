# Investigación técnica — consulta 6 de octubre de 2026

Se consultaron fuentes primarias antes de implementar. La documentación oficial
puede conservar fechas antiguas: se comprobó su contenido disponible y los
samples actuales; no se dedujo vigencia sólo por la fecha de publicación.

| Área | Hallazgo comprobado | Decisión del candidato |
|---|---|---|
| Game Bar | El widget documentado es UWP XAML, activado con ms-gamebarwidget | C# UWP; no tratarlo como widget de Windows 11 ni app WinUI 3 |
| Registro | Contrato microsoft.gameBarUIExtension en el manifest, propiedades de ventana y proxy/stub | Manifest explícito, SDK fijado al sample actual |
| Overlay | PinningSupported; RequestedOpacity 0–100; ClickThroughEnabled | Respetar controles del host y convertir opacity a 0–1 |
| IPC | Microsoft documenta mismo paquete para simplificar distribución e IPC; App Services es una opción | Core y widget en un MSIX, sin puerto de red |
| PresentMon | Captura externa y análisis de Present/display vía ETW; existe consola standalone | Child process filtrado por PID, pipe bloqueante |
| PresentMon actual | Releases muestra 2.6.0 publicada el 21 de septiembre; paquete oficial obtenido | Pin 2.6.0 con SHA-256; no instalar la UI ni servicio de Intel |
| FG | FrameType necesita instrumentación; displayed FPS puede incluir frames generados | No dividir ni multiplicar por dos; base/output no garantizados |
| DXGI waitable object | Requiere creación del swapchain con el flag correspondiente | No apropiarse de un swapchain externo; no sirve por sí solo como limiter universal |
| Timers | Windows dispone de CREATE_WAITABLE_TIMER_HIGH_RESOLUTION | QPC + timer privado + pequeño spin opcional |
| Detección | SetWinEventHook fuera de contexto se entrega mediante message loop | Detección por eventos, sin escaneo permanente en idle |
| VRR | Tearing support es una capacidad de la plataforma, no prueba de G-Sync activo | Refresco medido; estado VRR UNKNOWN |
| MSIX | Firma y confianza del certificado son necesarias para sideload normal | Build produce candidato sin firmar; signing separado |

## Cobertura de render APIs

DX11 y DX12 usan DXGI para presentación, pero obtener un punto de hook desde un
swapchain DX11 no prueba que se intercepte un swapchain DX12 o un proxy de FG.
Vulkan tiene vkQueuePresentKHR y extensiones de timing; un layer necesita
negociación, despacho y registro específicos. OpenGL usa presentación WGL/
SwapBuffers, que requiere otro punto de integración. Se investigaron esas rutas;
no se implementaron adaptadores incompletos fingiendo soporte.

Flip model, composed flip e independent flip cambian el camino hacia el display.
Contar Presents no demuestra cuántas imágenes nuevas vio el usuario. DWM,
composición, frames descartados y FG pueden separar ambos ritmos. El candidato
etiqueta la métrica principal como PRESENT FPS y preserva información de display
cuando existe; no afirma medir input-to-photon ni latencia total.

## Elección del limiter

Sleep solo tiene jitter y cuantización. Un timer privado duerme hasta cerca de
la fecha; el spin final mejora precisión a cambio de CPU. QPC ofrece la base de
tiempo. No se usa timeBeginPeriod global ni drivers. El deadline vive en el hilo
que llama a Present; dormir el Core externo no limita el juego.

No se copió código de RTSS, Special K, FramePacer ni limitadores de drivers.
El planificador y el controlador son propios. MinHook aporta el mecanismo de
hook bajo licencia BSD; no aporta el algoritmo de pacing.

## VRR, VSync y FG

165 Hz menos 3 FPS da 162 como ejemplo de margen, NO como ley de seguridad.
La cola, VSync, Reflex, rango VRR y FG determinan el comportamiento real. No se
cambia VSync del juego ni los ajustes del driver. Se requiere un perfil explícito
y prueba A/B. DLSS/Reflex y FSR FG tienen integración y pacing propios; un limiter
colocado en el sitio equivocado puede limitar el output en vez del render base.

El candidato bloquea pacing si sospecha componentes FG y exige confirmación
manual FG off antes de Advanced. Lossless Scaling es otro proceso y no queda
cubierto por una medición del PID del juego. Community Shaders y FG ×2 necesitan
pruebas y adaptadores posteriores. No se anuncia compatibilidad FG funcional.

## Fuentes primarias

- [Game Bar overview](https://learn.microsoft.com/en-us/xbox/game-bar/overview)
- [Manifest y registro](https://learn.microsoft.com/en-us/xbox/game-bar/guide/pkg-manifest)
- [IPC y desktop apps](https://learn.microsoft.com/en-us/xbox/game-bar/guide/communicating-apps)
- [XboxGameBarWidget API](https://learn.microsoft.com/en-us/xbox/game-bar/api/xgb-widget)
- [Samples Microsoft](https://github.com/microsoft/XboxGameBarSamples)
- [PresentMon consola](https://github.com/GameTechDev/PresentMon/blob/main/README-ConsoleApplication.md)
- [PresentMon releases](https://github.com/GameTechDev/PresentMon/releases)
- [DXGI frame latency object](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_3/nf-dxgi1_3-idxgiswapchain2-getframelatencywaitableobject)
- [Waitable timer](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-createwaitabletimerexw)
- [SetWinEventHook](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwineventhook)
- [DXGI VRR](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/variable-refresh-rate-displays)
- [Vulkan presentation](https://github.khronos.org/Vulkan-Site/refpages/latest/refpages/source/vkQueuePresentKHR.html)
- [Vulkan present timing](https://github.khronos.org/Vulkan-Site/features/latest/features/proposals/VK_EXT_present_timing.html)
- [NVIDIA Reflex integration](https://github.com/NVIDIA-RTX/Streamline/blob/main/docs/ProgrammingGuideReflex.md)
- [FSR3 integration](https://gpuopen.com/fidelityfx-super-resolution-3/)
- [MSIX signing](https://learn.microsoft.com/en-us/windows/msix/package/sign-msix-package-guide)
- [Intel security advisory](https://www.intel.com/content/www/us/en/security-center/advisory/intel-sa-01392.html)

La recomendación de Intel para INTEL-SA-01392 es 2.3.1 o posterior. Se eligió 2.6.0.
La huella del binario descargado es evidencia de integridad reproducible de esta
copia, no reemplaza Authenticode ni una auditoría de dependencias.
