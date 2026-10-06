# Desinstalación

Cierra el juego si utilizaste Advanced Pacing. Después:

```powershell
.\Installer\Uninstall.ps1
```

El script retira el paquete FramePulse del usuario actual y detiene sólo su Core
si pertenece a la ruta de ese paquete. Windows elimina el widget y el registro
StartupTask del paquete. No hay driver ni servicio de FramePulse instalado.

Se pregunta si deseas conservar `%LOCALAPPDATA%\FramePulse` (perfiles y logs).
Para eliminación explícita sin la pregunta:

```powershell
.\Installer\Uninstall.ps1 -DeleteProfiles
```

También puedes desinstalar desde Configuración → Aplicaciones, pero ese flujo no
pregunta por los perfiles externos y puede conservarlos. Bórralos manualmente si
lo deseas. Si confiaste en un certificado de desarrollo, retíralo posteriormente
de Trusted People identificándolo por su thumbprint; no borres otros certificados.

Si el juego sigue abierto, el heartbeat expira y el hook deja de limitar, pero
el módulo permanece en memoria hasta cerrar el juego. El desinstalador no fuerza
el cierre del juego ni descarga un DLL potencialmente ejecutándose.
