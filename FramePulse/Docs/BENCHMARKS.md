# Pruebas y rendimiento

## Resultados ejecutados aquí

Entorno Linux x64, g++ C++20; no corresponde al Acer Nitro ni a Windows.

| Comprobación | Resultado |
|---|---|
| Compilación portable -O2, Wall/Wextra/Wpedantic/Werror | Pasa |
| Reglas adaptativas, estadísticas, CSV y scheduler | 32 comprobaciones pasan |
| AddressSanitizer + UndefinedBehaviorSanitizer | Pasa, detección de fugas desactivada |
| LeakSanitizer | No ejecutable en el entorno instrumentado: acceso a /proc/ptrace |
| XML de proyectos/XAML/manifest | Bien formado; NO validación de esquema Windows |
| Integridad PresentMon | PE MZ y SHA-256 fijo verificados |
| 600 s sintéticos a 120 FPS, 72.000 frames | 6.28 ms de tiempo total, ejecución observada -O2 |

La simulación no duerme, no hace Present y no captura ETW. Su tiempo mide sólo
inserción de muestras, decisiones y estadísticas cortas. No permite afirmar que
el producto consume <1% CPU ni mejora el 1% low de un juego. No incluye el coste
de snapshots/UI, PresentMon o el hook. El número varía con host y compilación.

## A/B requerido en el Nitro

Hardware objetivo: i9-13900H, RTX 4060 Laptop ~75 W, 32 GB, 1080p/165 Hz.
Usa un recorrido reproducible y la misma carga de mods/guardado/clima. Fija
potencia y configuración térmica sin alterar clocks entre corridas.

1. Calienta el juego y descarta carga inicial; captura tres pasadas de 60–120 s.
2. A: FramePulse cerrado. B: Safe Mode. C: Manual al mismo cap de referencia.
3. D: Adaptive, mostrando estados y cap. Mantén FG apagado en estas pruebas.
4. Recoge PresentMon con los mismos ajustes en todos los casos; no compares una
   corrida con telemetría contra otra sin el coste equivalente de captura.
5. Alterna A/B para reducir sesgo térmico. Registra FPS, mean/median/P95/P99,
   1% low, drops, CPU y working set de Core/widget/PresentMon.
6. Mide GPU engines con herramientas Windows/driver; la composición del widget
   puede consumir GPU. El hilo del hook está dentro del juego y no aparece como
   CPU de FramePulseCore: incluir CPU del juego es obligatorio.
7. Repite idle con widget cerrado y abierto; Usa Tools/Benchmark.ps1 para CPU/RAM.

`Tools/AnalyzeCsv.py` resume frametimes offline. Los logs del Core contienen
estadísticas ya resumidas: su 1% low offline no sustituye el de una captura por frame.

## Registro todavía vacío

| Escenario | FPS / 1% low | CPU | GPU | RAM | Estado |
|---|---|---|---|---|---|
| Windows idle | Sin medir | Sin medir | Sin medir | Sin medir | Pendiente |
| Skyrim baseline | Sin medir | Sin medir | Sin medir | Sin medir | Pendiente |
| Skyrim Safe Mode | Sin medir | Sin medir | Sin medir | Sin medir | Pendiente |
| Skyrim Manual | Sin medir | Sin medir | Sin medir | Sin medir | Pendiente |
| Skyrim Adaptive | Sin medir | Sin medir | Sin medir | Sin medir | Pendiente |

Objetivos <1% CPU activo, ~0% idle y <100 MB son objetivos del diseño, no resultados.
