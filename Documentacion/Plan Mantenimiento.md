# Plan de Mantenimiento - ExtreamFS
## Angel Emanuel Rodriguez Corado - 202404856

---

## 1. Descripción General

Este documento describe las acciones necesarias para mantener el sistema ExtreamFS funcional, estable y extensible a lo largo del tiempo, así como las mejoras y nuevas funcionalidades que podrían incorporarse en versiones futuras.

---

## 2. Mantenimiento Correctivo

Son las correcciones que deben aplicarse cuando se detectan errores o comportamientos inesperados.

| Área | Acción |
|------|--------|
| Errores de compilación en nuevas versiones de g++ | Actualizar sintaxis C++ según el estándar vigente |
| Incompatibilidades con versiones de Graphviz | Verificar compatibilidad del lenguaje DOT generado |
| Corrupción de archivos `.mia` | Agregar validación de integridad con checksum en el MBR |
| Pérdida de particiones montadas al reiniciar | Implementar persistencia del mapa de montaje en disco |

---

## 3. Mantenimiento Preventivo

Acciones periódicas para evitar problemas antes de que ocurran.

- Compilar el proyecto con flags de advertencia `-Wall -Wextra` y corregir todos los warnings
- Verificar que `httplib.h` esté actualizado a la última versión estable
- Probar el flujo completo del script de calificación después de cualquier cambio
- Documentar cualquier nuevo comando o cambio en las estructuras de datos

---

## 4. Mejoras a Corto Plazo

Funcionalidades que pueden implementarse con esfuerzo moderado y que mejorarían la experiencia actual.

- **Comando `rm`** — eliminar archivos del sistema EXT2, liberando inodos y bloques en el bitmap
- **Comando `rename`** — renombrar archivos y directorios
- **Comando `cp`** — copiar archivos dentro del mismo disco
- **Persistencia de montaje** — guardar el estado de particiones montadas en un archivo `.json` para recuperarlo al reiniciar el servidor
- **Validación de permisos** — respetar `i_perm` al ejecutar operaciones según el usuario activo

---

## 5. Mejoras a Largo Plazo

Extensiones más complejas que ampliarían significativamente las capacidades del sistema.

- **Soporte para EXT3** — agregar journaling para recuperación ante fallos
- **Particiones lógicas en `mount`** — ya soportadas en `fdisk`, completar integración total
- **Interfaz gráfica mejorada** — reemplazar el editor de texto plano por un explorador visual de archivos y directorios
- **Modo multiusuario** — permitir múltiples sesiones simultáneas con diferentes particiones
- **API REST completa** — exponer cada comando como un endpoint individual en lugar de procesar texto plano
- **Tests automatizados** — crear un conjunto de pruebas unitarias con `catch2` o `gtest` para validar cada comando

---

## 6. Gestión de Versiones


| Versión | Descripción |
|---------|-------------|
| `v1.0` | Versión actual entregada en el curso |
| `v1.1` | Correcciones menores y persistencia de montaje |
| `v1.2` | Comandos `rm`, `rename` y `cp` |
| `v2.0` | Soporte EXT3 e interfaz gráfica mejorada |

---

## 7. Conclusión

Para mi el sistema ExtreamFS tiene una base sólida que permite extenderlo de forma incremental. Las mejoras prioritarias son la persistencia del estado de montaje y los comandos de eliminación y copia, ya que son los que más impactan la usabilidad diaria del simulador.

---

*Plan de Mantenimiento — ExtreamFS | Manejo e Implementación de Archivos | USAC 2026*