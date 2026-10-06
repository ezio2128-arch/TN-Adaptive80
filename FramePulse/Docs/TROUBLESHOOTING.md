# Troubleshooting

| Síntoma | Revisar |
|---|---|
| MSBuild no encuentra WindowsXaml/UAP | Componentes UWP C#, .NET Native y SDK 26100; no basta WinUI |
| Error SDKReference WindowsDesktop | Instalar el SDK exacto configurado o ajustar ambos proyectos consistentemente |
| MSIX no instala | Firma, Subject/Publisher, certificado confiable y frameworks x64 |
| FramePulse no aparece en Win+G | Confirmar instalación del paquete, Game Bar habilitada, contrato y manifest generado |
| CONNECTING | Ejecutar desde el paquete; revisar fullTrustProcess y AppService.Bridge; relanzar widget |
| WAITING FOR TELEMETRY | PresentMon.exe en Core, ETW/permisos, header actual, PID correcto |
| PRESENT FPS difiere de otra app | Otra métrica, otro swapchain, FG, frames descartados o intervalo de medida |
| Cap cambia en UI pero juego no | Safe Mode sólo aconseja; confirmar PACING ACTIVE y attachment explícito |
| PACING UNAVAILABLE | Adaptador no admitido, hook sin actividad, política, acceso denegado o FG sospechado |
| Juego desconocido | Añadir exe a games.txt en la carpeta de datos; esto sólo habilita reconocimiento/telemetría |
| No limita Vulkan/RDR2 | No hay adaptador Vulkan; DXGI experimental no implica todas las APIs |
| FG muestra UNKNOWN | El PID/telemetría no identifica de forma fiable base/output; no introducir multiplicador supuesto |
| Overlay pierde clicks | Game Bar click-through; desactivarlo para editar controles |
| Cap oscila en recuperación | Desactivar limitadores múltiples; preset Smooth; exportar debug.csv |
| Game Bar cerrada y Core sigue | Comportamiento intencional; detener Core desde widget o desinstalar |
| DLL sigue cargado en Safe Mode | Dormido hasta terminar el juego; no se realiza unload en caliente |

No desactives un anti-cheat, antivirus ni protecciones del sistema para hacer
funcionar este candidato. Un bloqueo significa usar Safe Mode o dejarlo sin usar.
Un scan fallido se informa como POLICY / INSPECTION, sin afirmar falsamente que se
detectó un anti-cheat concreto.

Logs opcionales: checkbox Debug log; hasta 18.000 filas a 5 Hz (una hora). La
siguiente activación sobrescribe debug.csv. El botón Open profiles / logs folder
abre la carpeta para copiar/exportar el archivo. No hay registro permanente por frame.
