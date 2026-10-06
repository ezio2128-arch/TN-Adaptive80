# Compatibilidad conocida

No hay juegos validados en Windows. Reconocido no significa compatible.

| Plataforma / juego | Telemetría | Limitador | Prueba real |
|---|---|---|---|
| Linux | Sólo tests portables | Scheduler portable | 32 checks pasan |
| Windows 11 x64 | Código preparado | Código preparado | No ejecutada |
| FramePulseTestGame DX11 | Reconocido | Permitido experimentalmente | No ejecutada |
| Skyrim SE/AE DX11, FG off | Reconocido | Permitido experimentalmente | No ejecutada |
| Skyrim Community Shaders / FG | Posible PresentMon; base/output desconocidos | Sin adaptador FG | No ejecutada |
| RDR2 | Reconocido | Sin adaptador Vulkan | No ejecutada |
| Cyberpunk 2077 DX12 | Reconocido | No permitido por adapter list | No ejecutada |
| The Witcher 3 DX11/DX12 | Reconocido | No permitido por adapter list | No ejecutada |
| Hogwarts Legacy | Reconocido | No permitido por adapter list | No ejecutada |
| Competitive / anti-cheat | Pasiva, si permisos/captura lo permiten | Bloqueado | Sin validación anti-cheat |
| Windows 10 | API base potencialmente viable | No soportado por manifest actual | No ejecutada |

El MSIX actual fija Windows.Desktop mínimo 22000 (Windows 11). Una variante
Windows 10 exigiría revisar manifest y probar herramientas/activación; no se
afirma soporte sólo porque algunas APIs existan allí.
