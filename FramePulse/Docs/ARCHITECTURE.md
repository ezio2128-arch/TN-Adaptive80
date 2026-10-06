# Arquitectura y decisiones

## Procesos

`FramePulseWidget` (UWP XAML, C#) y `FramePulseCore` (Win32, C++20) viven en el
mismo MSIX. El widget lanza Core con FullTrustProcessLauncher. Core abre una
conexión App Services al endpoint FramePulse.Bridge del paquete. El endpoint
comparte la conexión con la vista del widget, que envía comandos al Core.

El widget solicita un snapshot cada 200 ms mientras está visible. El Core
mantiene su vida independiente: cerrar Game Bar no envía shutdown. No hay
socket TCP, servidor HTTP, Electron, Chromium ni driver instalado.

Un mutex de sesión evita duplicar el Core. Una segunda activación señala un
evento de reconexión al proceso existente. El Core usa un message loop bloqueante
cuando no hay juego; no mantiene una captura ETW en idle.

## Detección y telemetría

SetWinEventHook observa cambios de ventana foreground, fuera del proceso objetivo.
Una lista de ejecutables reconoce juegos. Al arrancar se revisan ventanas visibles
para encontrar un juego que ya estaba abierto. El handle del juego se registra
con RegisterWaitForSingleObject y al terminar se libera la captura y el mapping.

Durante una sesión se ejecuta PresentMon.exe con filtro PID, salida por pipe y
sesión ETW propia. Un lector bloqueante decodifica columnas por nombre y entrega
muestras al motor. No se lee un CSV en disco ni se captura la pantalla.

Las ventanas estadísticas usan timestamps de eventos, evitando tratar una
entrega ETW por lotes como un pico de renderizado. El primer swapchain observado
queda seleccionado hasta cerrar la sesión: esta elección requiere mejora para
juegos con varios swapchains.

## Pacing

Advanced Pacing requiere una acción explícita en el widget o helper. El helper
verifica el nombre admitido, arquitectura x64, policy y la existencia del mapping.
Carga FramePulsePacing.dll mediante LoadLibraryW. MinHook conecta las funciones
DXGI Present y Present1 obtenidas de un swapchain de prueba DX11.

El DLL usa deadlines absolutos, un waitable timer de alta resolución y un spin
final de 50 µs. No modifica syncInterval ni flags del juego; ignora TEST y
DO_NOT_WAIT. Tras una pausa larga reinicia la cadencia, sin ráfagas de catch-up.
No se suspende el proceso completo ni se altera la prioridad del juego.

El mapping contiene cap en milésimas de FPS, heartbeat, ventana destino,
actividad del hook y versión de protocolo. El Core escribe con Interlocked;
el DLL verifica el heartbeat y deja de esperar tras dos segundos sin Core.
Safe Mode desactiva el mapping inmediatamente. El DLL queda dormido hasta cerrar
el juego; no se intenta descargar código mientras pueda estar en ejecución.

## Adaptive

Ventana de decisión: 1 s; resumen/gráfico: hasta 10 s; decisión: máximo 5 Hz.
La mediana determina FPS sostenibles, sin usar una única muestra lenta. P95/P99
y desviación describen el tail y las interrupciones. El 1% low es el inverso
de la media del 1% de frametimes más lentos, no simplemente el recíproco de P99.

| Preset | Confirmación de bajada | Estabilidad antes de sondeo |
|---|---:|---:|
| Smooth | 1.5 s | 5 s |
| Balanced | 1.0 s | 4 s |
| Responsive | 0.8 s | 3 s |

Estos tiempos son parámetros iniciales probados con simulaciones, no resultados
psicofísicos. La ventana añade retraso a la detección inicial.

Se baja cuando la capacidad mediana cae por debajo de 94% del cap. El nuevo cap
es floor(capacidad × 0.98), acotado a min/max. Por debajo del mínimo se muestra
LIMITED: el programa no puede crear capacidad de renderizado.

Una recuperación prueba un incremento de min(10 FPS, 10% del cap) durante 1.2 s.
Si la mediana o P95 no lo sostienen, vuelve al target anterior y dobla cooldown.
Las sondas hacen visibles pequeñas variaciones; se aceptan para descubrir
headroom sin inventarlo. Los valores son continuos con bajadas enteras, sin tiers
atados a supuestos multiplicadores de FG.

## Seguridad y perfiles

Las listas locales añaden reglas de bloqueo; no eliminan las reglas integradas.
Se inspeccionan procesos, módulos y servicios activos. Una inspección denegada
bloquea pacing. Una lista negativa no demuestra ausencia de anti-cheat.
La carga del helper no solicita elevación ni intenta saltarse restricciones.

App Services valida la identidad del paquete llamante. Los mappings utilizan
la DACL del token del usuario; no se abren a Everyone. Software que ejecuta con
el mismo usuario puede interferir: este IPC no constituye un aislamiento frente
a aplicaciones hostiles del mismo usuario.

Perfiles: `%LOCALAPPDATA%\FramePulse\Profiles\<exe>.json`. Escritura temporal y
MoveFileEx con reemplazo. La clave actual es el nombre de exe, por lo que dos
instalaciones con el mismo nombre comparten perfil. Futuro: clave por ruta/hash.

## Modularidad posterior

Telemetry/Csv.hpp puede sustituirse por integración directa PresentData/SDK sin
cambiar Adaptive. El limiter necesita adaptadores propios de API/FG: el DLL DXGI
actual no es un adaptador universal. Auto Learn, clasificación de streams y
adaptadores de FG quedan para posteriores versiones.
