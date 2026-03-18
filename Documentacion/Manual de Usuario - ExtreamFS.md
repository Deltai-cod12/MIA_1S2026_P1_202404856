# Manual de Usuario - ExtreamFS
## Angel Emanuel Rodriguez Corado - 202404856

---

## Tabla de Contenidos

1. [Introducción](#1-introducción)
2. [Requisitos del Sistema](#2-requisitos-del-sistema)
3. [Instalación y Configuración](#3-instalación-y-configuración)
4. [Interfaz del IDE](#4-interfaz-del-ide)
5. [Uso del Editor de Comandos](#5-uso-del-editor-de-comandos)
6. [Comandos del Sistema](#6-comandos-del-sistema)
7. [Generación de Reportes](#7-generación-de-reportes)
8. [Pasos Detellados](#8-pasos-detallados)
9. [Resolución de Problemas Comunes](#9-resolución-de-problemas-comunes)

---

## 1. Introducción

ExtreamFS es un simulador de sistema de archivos EXT2 que permite crear y administrar discos virtuales, particiones, usuarios y archivos a través de una interfaz web. El sistema está compuesto por un backend en C++ y un frontend accesible desde el navegador.

Este manual explica paso a paso cómo instalar, configurar y utilizar el sistema.

---

## 2. Requisitos del Sistema

| Requisito | Versión mínima |
|-----------|---------------|
| Sistema operativo | Ubuntu 20.04 o superior |
| Compilador | g++ 17 |
| Graphviz | Cualquier versión reciente |
| Navegador | Chrome, Firefox o Edge |
| RAM | 512 MB mínimo |

---

## 3. Instalación y Configuración

### Paso 1 — Instalar dependencias

Abrir una terminal y ejecutar:

```bash
sudo apt update
sudo apt install g++ graphviz
```

### Paso 2 — Compilar el backend

```bash
cd "Proyecto 1 Archivos/backend/src"
g++ -std=c++17 -o main main.cpp models/mounted_partitions.cpp -lstdc++fs -lpthread
```

Si la compilación es exitosa, aparecerá el archivo ejecutable `main` en la carpeta.

### Paso 3 — Iniciar el servidor

```bash
./main
```

La terminal mostrará:
```
=== MIA Backend en http://localhost:8080 ===
Abre http://localhost:8080/index.html en el navegador
Ctrl+C para detener
```

### Paso 4 — Abrir el IDE en el navegador

Abrir el navegador e ingresar la dirección:
```
http://localhost:8080/index.html
```

![Pantalla de inicio del IDE](ide_screenshot.png)

> **Nota:** El servidor debe estar corriendo en la terminal mientras se usa el IDE. No cerrar la terminal durante el uso.

---

## 4. Interfaz del IDE

![Vista general del IDE](ide_screenshot.png)

El IDE está dividido en cuatro secciones principales:

### 4.1 Barra de herramientas (superior)

![Barra de herramientas](capturas03_toolbar.png)

| Botón | Función |
|-------|---------|
| **Limpiar** | Borra el contenido del editor |
| **Cargar .smia** | Carga un archivo de script desde el equipo |
| **Guardar** | Descarga el contenido del editor como archivo `.smia` |
| **▶ Ejecutar** | Envía los comandos al backend y muestra la salida |
| **Limpiar consola** | Borra el contenido de la consola de salida |

### 4.2 Editor de comandos (izquierda)

![Editor de comandos](captura04_editor.png)

- Área de texto con numeración de líneas
- Soporta múltiples comandos (uno por línea)
- Las líneas que comienzan con `#` son comentarios y se muestran en la consola sin ejecutarse
- Tiene scroll interno para scripts largos

### 4.3 Consola de salida (derecha superior)

![Consola de salida](captura05_consola.png)

Muestra la respuesta del backend con colores según el tipo de mensaje:

| Color | Significado |
|-------|-------------|
| Azul | Comando ejecutado (`>>`) |
| Blanco | Salida normal |
| Amarillo | Encabezados de sección (`===`) |
| Rojo | Errores |
| Gris | Comentarios y mensajes informativos |

### 4.4 Visor de reportes (derecha inferior)

![Visor de reportes](captura06_visor_reportes.png)

- Muestra automáticamente los reportes generados con el comando `rep`
- Las imágenes (`.jpg`, `.png`) se renderizan directamente
- Los archivos de texto (`.txt`) se muestran con scroll
- Cada reporte tiene un botón **Abrir** para verlo en pantalla completa

---

## 5. Uso del Editor de Comandos

### Escribir comandos directamente

![Escribiendo comandos](captura07_escribir_comandos.png)

1. Hacer clic en el área del editor
2. Escribir el comando deseado
3. Presionar **▶ Ejecutar**

### Cargar un archivo de script

![Cargando archivo smia](captura08_cargar_archivo.png)

1. Presionar el botón **Cargar .smia**
2. Seleccionar el archivo `.smia` o `.txt` desde el explorador
3. El contenido se cargará automáticamente en el editor
4. Presionar **▶ Ejecutar** para ejecutar todos los comandos

### Usar comentarios

Las líneas que comienzan con `#` son comentarios. Se muestran en la consola pero no se ejecutan como comandos:

```bash
# Crear el disco principal
mkdisk -size=50 -unit=M -path=~/Discos/Disco1.mia

# Crear particiones
fdisk -type=P -name=Part1 -size=10 -unit=M -path=~/Discos/Disco1.mia
```

---

## 6. Comandos del Sistema

### 6.1 Gestión de Discos

#### Crear un disco — `mkdisk`

Crea un archivo de disco virtual con extensión `.mia`.

```
mkdisk -size=<n> -unit=<B|K|M> -fit=<BF|FF|WF> -path=<ruta>
```

**Ejemplo:**
```bash
mkdisk -size=50 -unit=M -fit=FF -path=~/Discos/Disco1.mia
```

![Resultado de mkdisk](captura09_mkdisk.png)

| Parámetro | Descripción | Obligatorio |
|-----------|-------------|-------------|
| `-size` | Tamaño del disco | Sí |
| `-unit` | Unidad: B (bytes), K (KB), M (MB) | No (default M) |
| `-fit` | Ajuste: BF (mejor), FF (primero), WF (peor) | No (default FF) |
| `-path` | Ruta donde se creará el disco | Sí |

#### Eliminar un disco — `rmdisk`

```
rmdisk -path=<ruta>
```

**Ejemplo:**
```bash
rmdisk -path=~/Discos/Disco1.mia
```

### 6.2 Gestión de Particiones

#### Crear partición — `fdisk`

```
fdisk -type=<P|E|L> -unit=<B|K|M> -name=<nombre> -size=<n> -path=<ruta> -fit=<BF|FF|WF>
```

**Ejemplos:**
```bash
# Partición primaria de 10MB
fdisk -type=P -unit=M -name=Part1 -size=10 -path=~/Discos/Disco1.mia -fit=FF

# Partición extendida de 30MB
fdisk -type=E -unit=M -name=PartE -size=30 -path=~/Discos/Disco1.mia -fit=FF

# Partición lógica de 10MB (dentro de la extendida)
fdisk -type=L -unit=M -name=PartL1 -size=10 -path=~/Discos/Disco1.mia -fit=FF
```

![Resultado de fdisk](captura10_fdisk.png)

#### Montar partición — `mount`

```
mount -path=<ruta> -name=<nombre_particion>
```

**Ejemplo:**
```bash
mount -path=~/Discos/Disco1.mia -name=Part1
```

El sistema asigna un ID de montaje automáticamente (ej. `561A`). Este ID se usa en los demás comandos.

#### Listar particiones montadas — `mounted`

```
mounted
```

![Resultado de mounted](captura11_mounted.png)

### 6.3 Formateo

#### Formatear con EXT2 — `mkfs`

```
mkfs -id=<id_montaje> -type=<full|fast>
```

**Ejemplo:**
```bash
mkfs -id=561A -type=full
```

- `full`: limpia completamente la partición antes de formatear
- `fast`: solo escribe las estructuras sin limpiar

![Resultado de mkfs](captura12_mkfs.png)

### 6.4 Sesión de Usuario

#### Iniciar sesión — `login`

```
login -user=<usuario> -pass=<contraseña> -id=<id_montaje>
```

**Ejemplo:**
```bash
login -user=root -pass=123 -id=561A
```

> Solo puede haber una sesión activa a la vez. El usuario `root` y contraseña `123` se crean automáticamente al formatear.

#### Cerrar sesión — `logout`

```
logout
```

### 6.5 Gestión de Grupos y Usuarios

> Todos estos comandos requieren sesión activa de `root`.

#### Crear grupo — `mkgrp`

```
mkgrp -name=<nombre>
```

#### Eliminar grupo — `rmgrp`

```
rmgrp -name=<nombre>
```

#### Crear usuario — `mkusr`

```
mkusr -user=<nombre> -pass=<contraseña> -grp=<grupo>
```

#### Eliminar usuario — `rmusr`

```
rmusr -user=<nombre>
```

#### Cambiar grupo de usuario — `chgrp`

```
chgrp -user=<nombre> -grp=<nuevo_grupo>
```

#### Ver contenido de users.txt — `cat`

```
cat -file1=/users.txt
```

![Gestión de usuarios](captura13_usuarios.png)

### 6.6 Gestión de Archivos y Directorios

#### Crear directorio — `mkdir`

```
mkdir -path=<ruta> [-p]
```

**Ejemplos:**
```bash
# Crear directorio (el padre debe existir)
mkdir -path=/home

# Crear directorio y todos sus padres automáticamente
mkdir -p -path=/home/archivos/user/docs
```

#### Crear archivo — `mkfile`

```
mkfile -path=<ruta> [-size=<n>] [-cont=<ruta_host>] [-r]
```

**Ejemplos:**
```bash
# Archivo relleno con secuencia 0-9 de 100 bytes
mkfile -path=/home/docs/archivo.txt -size=100

# Archivo con contenido de un archivo del equipo
mkfile -path=/home/docs/nombre.txt -cont=/home/angel/NAME.txt

# Crear archivo y sus directorios padre automáticamente
mkfile -r -path=/home/docs/subdir/archivo.txt -size=50
```

#### Leer archivo — `cat`

```
cat -file1=<ruta_en_disco>
```

**Ejemplo:**
```bash
cat -file1=/home/docs/archivo.txt
```

![Resultado de cat](captura14_cat.png)

---

## 7. Generación de Reportes

Los reportes se generan con el comando `rep` y aparecen automáticamente en el panel derecho del IDE.

```
rep -id=<id_montaje> -path=<ruta_salida> -name=<tipo> [-path_file_ls=<ruta>]
```

### 7.1 Reporte MBR

Muestra la estructura del MBR y las particiones del disco.

```bash
rep -id=561A -path=~/Reportes/mbr.jpg -name=mbr
```

![Reporte MBR](captura15_rep_mbr.jpg)

### 7.2 Reporte DISK

Muestra la distribución proporcional del disco con porcentajes.

```bash
rep -id=561A -path=~/Reportes/disk.jpg -name=disk
```

![Reporte DISK](captura16_rep_disk.jpg)

### 7.3 Reporte INODE

Muestra todos los inodos en uso con sus campos.

```bash
rep -id=561A -path=~/Reportes/inode.jpg -name=inode
```

![Reporte INODE](captura17_rep_inode.jpg)

### 7.4 Reporte BLOCK

Muestra todos los bloques en uso (carpeta, archivo, apuntadores).

```bash
rep -id=561A -path=~/Reportes/block.jpg -name=block
```

![Reporte BLOCK](captura18_rep_block.jpg)

### 7.5 Reporte BM_INODE y BM_BLOCK

Genera un archivo `.txt` con el bitmap de inodos o bloques (20 bits por línea).

```bash
rep -id=561A -path=~/Reportes/bm_inode.txt -name=bm_inode
rep -id=561A -path=~/Reportes/bm_block.txt -name=bm_block
```

![Reporte BM_INODE](captura19_rep_bminode.png)

### 7.6 Reporte SUPERBLOQUE

Muestra todos los campos del superbloque de la partición.

```bash
rep -id=561A -path=~/Reportes/sb.jpg -name=sb
```

![Reporte SB](captura20_rep_sb.jpg)

### 7.7 Reporte TREE

Genera el árbol completo del sistema de archivos desde la raíz.

```bash
rep -id=561A -path=~/Reportes/tree.png -name=tree
```

![Reporte TREE](captura21_rep_tree.png)

### 7.8 Reporte FILE

Muestra el nombre y contenido de un archivo específico del disco.

```bash
rep -id=561A -path=~/Reportes/archivo.txt -name=file -path_file_ls=/home/docs/archivo.txt
```

### 7.9 Reporte LS

Muestra el listado de archivos y carpetas de un directorio con permisos.

```bash
rep -id=561A -path=~/Reportes/ls.jpg -name=ls -path_file_ls=/home/docs
```

![Reporte LS](captura22_rep_ls.jpg)

---

## 8. Pasos Detallados

A continuación se muestra el flujo completo típico de uso del sistema:

### Paso 1 — Crear y particionar el disco


```bash
# Crear disco de 50MB
mkdisk -size=50 -unit=M -fit=FF -path=~/Discos/Disco1.mia

# Crear 2 particiones primarias de 10MB
fdisk -type=P -unit=M -name=Part1 -size=10 -path=~/Discos/Disco1.mia -fit=FF
fdisk -type=P -unit=M -name=Part2 -size=10 -path=~/Discos/Disco1.mia -fit=FF
```

### Paso 2 — Montar y formatear


```bash
# Montar partición
mount -path=~/Discos/Disco1.mia -name=Part1

# Formatear con EXT2
mkfs -id=561A -type=full
```

### Paso 3 — Iniciar sesión y crear usuarios


```bash
login -user=root -pass=123 -id=561A

mkgrp -name=estudiantes
mkusr -user=angel -pass=1234 -grp=estudiantes
```

### Paso 4 — Crear estructura de archivos


```bash
mkdir -p -path=/home/angel/documentos
mkfile -path=/home/angel/documentos/tarea.txt -size=100
cat -file1=/home/angel/documentos/tarea.txt
```

### Paso 5 — Generar reportes


```bash
rep -id=561A -path=~/Reportes/disk.jpg     -name=disk
rep -id=561A -path=~/Reportes/tree.png     -name=tree
rep -id=561A -path=~/Reportes/inode.jpg    -name=inode
rep -id=561A -path=~/Reportes/sb.jpg       -name=sb
```

---

## 9. Resolución de Problemas Comunes

### El servidor no inicia

**Síntoma:** Al ejecutar `./main` aparece un error o no muestra el mensaje del servidor.

**Solución:**
1. Verificar que se compiló correctamente: `ls -la main`
2. Verificar que el puerto 8080 no está en uso: `lsof -i :8080`
3. Si está en uso: `kill $(lsof -t -i:8080)` y reiniciar

### El IDE muestra "Error de conexión"


**Síntoma:** Al presionar Ejecutar aparece "Error de conexión con el backend."

**Solución:**
1. Verificar que `./main` está corriendo en la terminal
2. Verificar la URL: debe ser exactamente `http://localhost:8080/index.html`
3. Revisar la consola del navegador (F12) para más detalles

### Error: "particion no montada"

**Síntoma:** Comandos como `mkfs`, `login`, `mkdir` retornan este error.

**Solución:** Ejecutar `mount` antes de usar el ID de partición:
```bash
mount -path=~/Discos/Disco1.mia -name=Part1
# Anotar el ID generado (ej. 561A) y usarlo en los demás comandos
```

### Error: "no hay sesion activa"

**Síntoma:** Comandos como `mkdir`, `mkfile`, `mkgrp` retornan este error.

**Solución:** Iniciar sesión primero:
```bash
login -user=root -pass=123 -id=561A
```

### Error: "Graphviz falló"

**Síntoma:** Los reportes no se generan y aparece este mensaje.

**Solución:** Instalar Graphviz:
```bash
sudo apt install graphviz
dot -V  # Verificar instalación
```

### Los reportes no aparecen en el visor

**Síntoma:** El comando `rep` dice "Reporte generado" pero no aparece en el panel.

**Solución:**
1. Verificar que la carpeta de destino existe: `mkdir -p ~/Reportes`
2. Verificar que la ruta del `-path` es absoluta (empieza con `/` o `~`)
3. Recargar la página del navegador (F5)

---

*Manual de Usuario — ExtreamFS | Manejo e Implementación de Archivos | USAC 2026*