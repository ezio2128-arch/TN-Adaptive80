# Diseño técnico v0.5

## Clasificación

La contribución GPU se estima como:

`smoothedGpuTime / smoothedPreFGFrameTime`

Orientación inicial:
- GPU: contribución alta y GPU por encima del presupuesto.
- Mixto: contribución intermedia y GPU todavía por encima del presupuesto.
- CPU/motor: contribución baja y GPU con margen.

Los timestamps D3D11 llegan retrasados. Si el frametime de referencia de la consulta y el frametime actual difieren demasiado, el timing se marca como no alineado y no puede promover un cambio de resolución.

## Transient Guard

El controlador puede detectar una caída en cada frame, pero solo aplica un resize cuando la presión GPU permanece durante `GPU Pressure Qualification`.

Esto busca filtrar picos por:
- giro rápido de cámara;
- streaming/LOD;
- draw calls transitorios;
- carga del motor.

## Performance Reserve

`Target Native FPS` se interpreta como suelo. `Performance Reserve FPS` define FPS adicionales que AD80 intenta conservar antes de empezar Slow Recovery de calidad.

Ejemplo Balanced:
- suelo: 40 FPS;
- reserva: +8;
- zona de rendimiento útil aproximada: 40–48+ FPS;
- recuperación de calidad solo cuando existe margen real por encima de esa zona.
