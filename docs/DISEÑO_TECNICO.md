# Diseño técnico v0.4

AD80 mantiene su controlador temporal v0.3, pero la ventana de escalas efectiva se intersecta con la ventana reportada por DLSS. Para una escala escalar, el mínimo seguro usa el mayor ratio de los ejes mínimos y el máximo seguro el menor ratio de los ejes máximos. Las dimensiones finales se alinean a múltiplos de 8 sin salir del rango del proveedor.

Si la consulta falla o DLSS no expone rango dinámico, AD80 se bloquea a una escala segura en lugar de continuar con valores arbitrarios.
