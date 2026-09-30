# ExtreamFS | EXT2 File System Simulator

**Repository name suggestion:** `extreamfs-filesystem-simulator`  
**Nombre en español:** `ExtreamFS — Simulador de sistema de archivos EXT2`  
**Description:** Educational EXT2 file-system simulator with virtual disks, partition management, a C++ REST backend and a browser interface.

## English

### Overview

ExtreamFS is an educational simulator that operates on virtual disk image files (`.mia`). It models disk partitioning and an EXT2-style file system, and exposes a browser interface backed by a local C++ HTTP service. It is designed to make on-disk structures and file-system operations inspectable through generated reports.

### Capabilities documented in the project

- Create and remove virtual disks; create primary, extended and logical partitions
- Mount partitions and format them with the project’s EXT2 implementation
- Create directories and files; read file contents
- Manage users and groups and run commands through a script editor
- Generate reports for structures such as the MBR, disk layout, superblock, inodes, blocks and directory tree
- Visualize report output using Graphviz

The `Fase2` branch includes additional command and journaling work. The default `main` branch and `Fase2` do not contain identical code; check out the branch whose implementation you intend to run.

### Architecture and technology

- C++17 backend using `cpp-httplib`
- HTTP endpoints on `localhost:8080`
- HTML, CSS and JavaScript interface
- Graphviz for visual reports
- Binary `.mia` disk images with MBR/EBR, superblock, inode, bitmap and block structures
- Documented development environment: Ubuntu and `g++`

### Requirements

- Linux environment (the manual documents Ubuntu)
- `g++` with C++17 support
- Graphviz

Install the documented packages on Ubuntu:

```bash
sudo apt update
sudo apt install g++ graphviz
```

### Build and run

From `backend/src` on the selected branch:

```bash
g++ -std=c++17 -o main main.cpp models/mounted_partitions.cpp -lpthread
./main
```

Then open `http://localhost:8080/index.html`. The manual also describes a report endpoint at `GET /reporte?path=...` and command execution through `POST /ejecutar`.

### Documentation

- [Technical manual](https://github.com/Deltai-cod12/MIA_1S2026_P1_202404856/blob/main/Documentacion/Manual%20tecnico%20-%20ExtreamFS.md)
- [User manual](https://github.com/Deltai-cod12/MIA_1S2026_P1_202404856/blob/main/Documentacion/Manual%20de%20Usuario%20-%20ExtreamFS.md)
- [Workflow diagram](https://github.com/Deltai-cod12/MIA_1S2026_P1_202404856/blob/main/Documentacion/Diagrama%20Flujo%20de%20Trabajo.md)

### Academic context

Developed for the File Management and Implementation course at Universidad de San Carlos de Guatemala.

## Español

### Descripción

ExtreamFS es un simulador educativo que trabaja con archivos de imagen de disco virtual (`.mia`). Modela el particionamiento y un sistema de archivos basado en EXT2, y ofrece una interfaz web respaldada por un servicio HTTP local en C++. El proyecto permite inspeccionar estructuras internas y operaciones mediante reportes.

### Funciones documentadas

- Crear y eliminar discos virtuales; crear particiones primarias, extendidas y lógicas
- Montar particiones y formatearlas con la implementación EXT2 del proyecto
- Crear directorios y archivos; leer el contenido de archivos
- Administrar usuarios y grupos, y ejecutar comandos desde un editor de scripts
- Generar reportes de estructuras como MBR, distribución del disco, superbloque, inodos, bloques y árbol de directorios
- Visualizar reportes con Graphviz

La rama `Fase2` incorpora trabajo adicional de comandos y journaling. La rama predeterminada `main` y `Fase2` no contienen exactamente el mismo código; selecciona la rama que quieres ejecutar.

### Arquitectura y tecnologías

- Backend C++17 con `cpp-httplib`
- Endpoints HTTP en `localhost:8080`
- Interfaz en HTML, CSS y JavaScript
- Graphviz para reportes visuales
- Imágenes binarias `.mia` con estructuras MBR/EBR, superbloque, inodos, mapas de bits y bloques
- Entorno de desarrollo documentado: Ubuntu y `g++`

### Requisitos

- Entorno Linux (el manual documenta Ubuntu)
- `g++` compatible con C++17
- Graphviz

Instala los paquetes documentados en Ubuntu:

```bash
sudo apt update
sudo apt install g++ graphviz
```

### Compilar y ejecutar

Desde `backend/src` en la rama elegida:

```bash
g++ -std=c++17 -o main main.cpp models/mounted_partitions.cpp -lpthread
./main
```

Luego abre `http://localhost:8080/index.html`. El manual también describe el endpoint de reportes `GET /reporte?path=...` y la ejecución de comandos mediante `POST /ejecutar`.

### Documentación

- [Manual técnico](https://github.com/Deltai-cod12/MIA_1S2026_P1_202404856/blob/main/Documentacion/Manual%20tecnico%20-%20ExtreamFS.md)
- [Manual de usuario](https://github.com/Deltai-cod12/MIA_1S2026_P1_202404856/blob/main/Documentacion/Manual%20de%20Usuario%20-%20ExtreamFS.md)
- [Diagrama de flujo de trabajo](https://github.com/Deltai-cod12/MIA_1S2026_P1_202404856/blob/main/Documentacion/Diagrama%20Flujo%20de%20Trabajo.md)

### Contexto académico

Desarrollado para el curso de Manejo e Implementación de Archivos de la Universidad de San Carlos de Guatemala.

---

**Topics:** `cpp`, `c-plus-plus`, `filesystem`, `ext2`, `operating-systems`, `disk-image`, `graphviz`, `rest-api`, `linux`, `systems-programming`
