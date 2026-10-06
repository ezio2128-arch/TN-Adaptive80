# Limitaciones y trabajo abierto

## Bloqueadores de release

- No se compilaron proyectos Windows; no hay Core/DLL/widget binarios propios.
- No se generó, firmó, instaló ni desinstaló MSIX en Windows.
- No se comprobó Game Bar ni su registro en Win+G.
- No se comprobó pacing real, coexistencia con overlays, estabilidad de hooks o overhead.
- No hay adaptación de FG, Vulkan, OpenGL ni DX12 validada.

## Producto actual

Safe Mode sólo mide y aconseja. Advanced es experimental y sólo permite SkyrimSE
como primer candidato y el harness DX11. Obtener direcciones Present desde un
swapchain dummy no garantiza cubrir proxies, otros dispositivos/APIs o todos los
swapchains. FG permanece UNKNOWN/confirmación de usuario; módulos conocidos son
indicios, no detección del estado activo. No se publican BASE/OUTPUT inventados.

No existe detección universal de limitadores del juego, driver, VSync o Reflex.
Sólo la presencia de RTSS produce una advertencia. No se puede prometer que el
limitador sea el único activo. No se altera VSync ni se configura G-Sync/FreeSync.
VRR activo/rango no se mide; se muestra UNKNOWN. No hay techo automático garantizado.

La política es conservadora pero no exhaustiva. Puede bloquear por falsos positivos
o acceso denegado. Servicios instalados pero inactivos y anti-cheats nuevos pueden
no detectarse. Un control cada segundo implica una ventana de detección; no se
intenta compatibilidad, evasión ni ocultación.

La selección del stream fija el primer swapchain. Los diferentes nombres de CSV
se resuelven por header, pero la compatibilidad con el ejecutable empaquetado
necesita captura real. La telemetría ETW puede tener retraso de entrega. Cambiar
el perfil reinicia las ventanas; se requieren nuevas muestras para Adaptive.

La precisión del timer, política de energía, composición y sincronización del
juego afectan el pacing. El spin consume CPU dentro del proceso del juego.
El objetivo GPU≈0 aplica al motor sin render; la UI del widget usa composición
Windows y no puede prometer un coste GPU literalmente cero.

El DLL no se descarga en caliente. Tras bloquear pacing queda inactivo en el
juego hasta que éste termina. El harness ocupa CPU deliberadamente y no sirve
para medir eficiencia del producto completo.

Los perfiles son por nombre exe. El slider de opacidad lo proporciona Game Bar;
no hay panel propio completo, selector rápido de presets de FPS, ni un modo
compacto dedicado (sí redimensionado). Inicio con Windows tiene código StartupTask
pero no validación. Auto Learn se difiere; no existe base de aprendizaje fingida.

## Próximos trabajos necesarios

1. Ejecutar MSBuild Windows y corregir errores reales, no sólo XML.
2. Validar App Services y ciclo de vida, incluyendo múltiples aperturas/cierres.
3. Probar harness, confirmar precisión del cap y medir coste del hook.
4. Probar Skyrim sin FG ni otro cap antes de añadir overlays y mods.
5. Selección robusta de swapchain y protocolos de cap/adaptador específicos.
6. Integración directa PresentData para reducir coste CSV si benchmarks lo requieren.
7. Adaptadores Vulkan/OpenGL/DX12 y FG con métricas verificables.
8. Firma de distribución, aceptación completa y release 1.0.0 real.
