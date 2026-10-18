# Living Lands — Bottled Shaders, fase 1 (experimental)

**Estado: menú y lógica fuente implementados; pruebas portables locales aprobadas. La compilación Windows (DLL SKSE) y las pruebas dentro del juego siguen pendientes de confirmación.**

Rama aislada de `TN-Adaptive80/main`. No modifica los archivos anteriores de AD80 v0.5.

## Base provisional
- Fuente: `InTheBottle/Bottled-Shaders`, rama `Bottle-Compendium`, commit `77b19a8a9fd5cc1032fc6e0f08815ec6e362bef0`.
- Archivos completos derivados de esa base: `overlay/src/Features/SnowCover.cpp`, `SnowCover.h`. Nuevo helper `SeasonalSnowPolicy.h`.
- **Comprobado contra ZIP del usuario**: MO2 indica descarga de Nexus Bottled Shaders 1.0.16 (17-oct-2026), mientras el proyecto fuente CMake muestra versión interna 1.9.0. Coinciden exactamente los SHA-256 de SnowCover.hlsli y SharedData.hlsli y FNV de UpdateParams.hlsli con el commit fijado.

## Menú implementado en código
Bottled Shaders > Sky & Weather > Snow Cover > Worldspace Config > Weather and Seasons:
- Enable seasonal snow controller;
- Follow Seasons of Skyrim (if installed);
- Winter: Snow Cover (ON);
- Spring: Snow Cover (OFF);
- Summer: Snow Cover (OFF);
- Autumn: Snow Cover (OFF: deja la nieve *vanilla* o de Seasons of Skyrim);
- detected current season and source.

**Guardar con `Worldspace Config > Save`**, no solo el botón global Save Settings.
Persistencia por JSON `Shaders/SnowCover/<worldspace>.json`. Las claves nuevas usan `config.value(..., default)` para no romper configuraciones anteriores.

## Mecanismo
La función exportada `GetCurrentSeason()` de `po3_SeasonsOfSkyrim.dll` comunica la estación real, incluidos overrides. Si no está disponible, se usa el mes vanilla, por lo que se pierden las asignaciones personalizadas; el menú informa de que es fallback. Cuando la estación está apagada:
- anula el acumulado por clima y la altitud de nieve;
- escribe `perFrame.wsettings.EnableSnowCover=0` sin desactivar el addon entero;
- evita alterar la nieve vanilla, los paisajes Unfrozen y Snow Deformation.

**Límites:** esto no quita nieve integrada en texturas, meshes o LODs. Deben verificarse las rutas de actualización de Snow Cover y los worldspaces con el paquete exacto. No hay DLL ni ZIP instalable aún. Requiere validación/compilación.

## Tests portables
`tests/SeasonalSnowPolicyTests.cpp` comprueba máscara estacional, fallback por mes y activación/desactivación. Ejecución sugerida: `c++ -std=c++20 -Wall -Wextra -pedantic tests/SeasonalSnowPolicyTests.cpp -o test-snow && ./test-snow`.

## Insumos de instalación requeridos
1. ZIP/7z original de Bottled Shaders instalado (o captura de nombre exacto, versión y opciones, más build ZIP).
2. `Data/SKSE/Plugins/po3_SeasonsOfSkyrim.ini` efectivo en el VFS de MO2.
3. Carpeta `Data/Shaders/SnowCover/` (JSON, mapa DDS, listas white/black) que realmente gana conflictos en MO2.
4. Versión del ejecutable Skyrim SE/AE y si el usuario emplea prueba en GTX 1650 o RTX 4060.
5. Opcional: `Data/Seasons/...` para worldspaces y datos estacionales en nuevos territorios.

**No sobrescribir DLL originales sin compilar el port exacto.**

## Compilación y distribución experimental
- Workflow `.github/workflows/bottled-phase1-build.yml` toma el commit fijado de Bottled Shaders y le aplica exclusivamente nuestro overlay de Snow Cover.
- Antes de compilar comprueba hashes SHA-256 de dos shaders de la instalación original del usuario, garantizando que la base seleccionada es compatible con esos archivos.
- Build Windows 2025 + MSVC 2026 + CMake preset `PR` (sin optimización LTO plena, diseñado para test).
- Artefacto esperado si todos los jobs finalizan correctamente: `Living-Lands-Bottled-Snow-Controller-Phase1-MO2.zip`.
- **NO instalado ni compilado todavía.** No sustituir la DLL de Skyrim hasta que CI termine verde.
- Instalar la futura **DLL-only override** debajo del Bottled 1.0.16 original en MO2, para no duplicar Effects11 y texturas.
- Si el mod tiene un archivo de configuración sobrescrito en el panel izquierdo de MO2, verificar las reglas de prioridad y copias de seguridad antes de reemplazar.
