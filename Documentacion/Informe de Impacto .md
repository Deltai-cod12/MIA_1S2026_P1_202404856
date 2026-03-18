# Informe de Impacto - ExtreamFS
## Angel Emanuel Rodriguez Corado - 202404856

---

## 1. Descripción del Contexto

ExtreamFS es un simulador del sistema de archivos EXT2 desarrollado como herramienta educativa para el curso de Manejo e Implementación de Archivos de la Universidad de San Carlos de Guatemala. El proyecto busca que los estudiantes comprendan el funcionamiento interno de un sistema de archivos real a través de la implementación práctica.

---

## 2. Impacto Educativo

El principal impacto del proyecto es académico. Al implementar desde cero estructuras como el MBR, EBR, inodos, bitmaps y bloques de datos, el estudiante adquiere un entendimiento profundo que no es posible obtener únicamente con teoría.

- **Comprensión práctica** del funcionamiento de EXT2, el sistema de archivos más utilizado en Linux
- **Refuerzo de conceptos** de estructuras de datos, manejo de archivos binarios y administración de memoria
- **Experiencia con herramientas reales** como Graphviz para visualización y cpp-httplib para comunicación HTTP
- **Desarrollo de habilidades** en C++17, arquitecturas cliente-servidor y diseño de APIs REST

---

## 3. Impacto Técnico

### Simulación sin riesgo
Al trabajar sobre archivos `.mia` en lugar de dispositivos reales, el estudiante puede experimentar libremente con particionamiento, formateo y corrupción de datos sin riesgo para el sistema operativo o el hardware.

### Visualización de estructuras internas
Los reportes generados con Graphviz permiten visualizar en tiempo real el estado interno del sistema de archivos — algo que normalmente requiere herramientas especializadas de análisis de disco. Esto reduce significativamente el tiempo necesario para depurar y entender el comportamiento del sistema.

### Arquitectura escalable
La separación entre backend C++ y frontend web permite que el proyecto sea extendido fácilmente en el futuro, por ejemplo agregando soporte para EXT3, journaling, o nuevos tipos de reportes, sin modificar la interfaz de usuario.

---

## 4. Impacto en Eficiencia

| Área | Sin el simulador | Con ExtreamFS |
|------|-----------------|---------------|
| Comprensión de inodos | Lectura teórica | Visualización interactiva con `rep inode` |
| Depuración de estructuras | Hexdump manual del disco | Reportes automáticos en JPG/PNG |
| Pruebas de particionamiento | Requiere máquina virtual | Ejecución directa sobre archivos `.mia` |
| Gestión de usuarios | Concepto abstracto | Implementación real en `users.txt` |

---

## 5. Limitaciones y Consideraciones

- El simulador no implementa journaling ni permisos avanzados de EXT2
- Al ser un simulador en memoria, los mapas de particiones montadas se pierden al reiniciar el servidor
- El rendimiento no es comparable con un sistema de archivos real al estar orientado a la comprensión y no a la optimización

---

## 6. Conclusión

ExtreamFS cumple su objetivo principal dado en nuestro proyecto: proporcionar una plataforma práctica y visual para entender el funcionamiento de un sistema de archivos EXT2. Su mayor impacto es educativo, reduciendo la brecha entre la teoría vista en clase y la implementación real que los estudiantes encontrarán en entornos profesionales de desarrollo de sistemas operativos y administración de sistemas Linux.

---

*Informe de Impacto — ExtreamFS | Manejo e Implementación de Archivos | USAC 2026*