   // Utilidades de consola

function escribirConsola(texto, tipo = "normal") {
    const consola = document.getElementById("console");

    const linea = document.createElement("div");
    linea.className = "console-line console-" + tipo;
    linea.textContent = texto;
    consola.appendChild(linea);
    consola.scrollTop = consola.scrollHeight;
}

function limpiarConsola() {
    const consola = document.getElementById("console");
    consola.innerHTML = "";
}

   // Editor

function nuevo() {
    document.getElementById("editor").value = "";
    limpiarConsola();
    actualizarLineas();
}

function cargarArchivo(event) {
    const file = event.target.files[0];
    if (!file) return;
    const reader = new FileReader();
    reader.onload = function(e) {
        document.getElementById("editor").value = e.target.result;
        actualizarLineas();
    };
    reader.readAsText(file);
    // Limpiar el input para permitir cargar el mismo archivo otra vez
    event.target.value = "";
}

function guardarCodigo() {
    const contenido = document.getElementById("editor").value;
    descargarTexto("script.smia", contenido);
}

   // Ejecutar

function ejecutar() {
    limpiarConsola();
    escribirConsola("Enviando comandos al servidor...", "muted");

    const codigo = document.getElementById("editor").value.trim();

    if (!codigo) {
        escribirConsola("No hay comandos para ejecutar.", "error");
        return;
    }

    // Indicador visual de carga
    const btn = document.querySelector(".btn-ejecutar");
    btn.disabled = true;
    btn.textContent = "Ejecutando...";

    fetch("http://localhost:8080/ejecutar", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ codigo: codigo })
    })
    .then(response => {
        if (!response.ok) throw new Error("HTTP " + response.status);
        return response.json();
    })
    .then(data => {
        const salida = data.salida || "";

        // Mostrar salida línea por línea con colores
        salida.split("\n").forEach(linea => {
            if (linea.startsWith(">>")) {
                escribirConsola(linea, "cmd");
            } else if (linea.toLowerCase().startsWith("error")) {
                escribirConsola(linea, "error");
            } else if (linea.startsWith("===")) {
                escribirConsola(linea, "header");
            } else if (linea.startsWith("#")) {
                escribirConsola(linea, "comment");
            } else {
                escribirConsola(linea, "normal");
            }
        });

        // Guardar para descarga
        window.ultimaSalida = salida;

        // Buscar reportes generados y mostrarlos
        detectarYMostrarReportes(salida);
    })
    .catch(err => {
        escribirConsola("Error de conexión con el backend.", "error");
        escribirConsola(err.toString(), "error");
        escribirConsola("Verifica que el servidor C++ esté corriendo en puerto 8080.", "muted");
    })
    .finally(() => {
        btn.disabled = false;
        btn.textContent = "▶ Ejecutar";
    });
}

 //  Visor de reportes
 //  Detecta rutas de imágenes/txt en la salida del backend
 //  y las muestra en el panel de reportes

function detectarYMostrarReportes(salida) {
    // Buscar patrones como "Reporte X generado: /ruta/archivo.jpg"
    const regex = /Reporte \w+ generado: (.+\.(jpg|jpeg|png|pdf|txt))/gi;
    let match;
    const reportesEncontrados = [];

    while ((match = regex.exec(salida)) !== null) {
        reportesEncontrados.push(match[1].trim());
    }

    if (reportesEncontrados.length === 0) return;

    const reportsBody = document.getElementById("reportsBody");
    const reportsEmpty = document.getElementById("reportsEmpty");

    if (reportsEmpty) reportsEmpty.style.display = "none";

    reportesEncontrados.forEach(ruta => {
        const ext = ruta.split(".").pop().toLowerCase();
        const nombre = ruta.split("/").pop();

        const item = document.createElement("div");
        item.className = "report-item";

        const header = document.createElement("div");
        header.className = "report-item-header";
        header.innerHTML = `<span class="report-name">${nombre}</span>
                            <a href="file://${ruta}" target="_blank" class="btn btn-sm btn-ghost">Abrir</a>`;

        item.appendChild(header);

        if (ext === "jpg" || ext === "jpeg" || ext === "png") {
            // Imagen: mostrar inline usando el endpoint /reporte
            const img = document.createElement("img");
            // Pedimos la imagen al backend
            img.src = `http://localhost:8080/reporte?path=${encodeURIComponent(ruta)}&t=${Date.now()}`;
            img.className = "report-img";
            img.alt = nombre;
            img.onerror = () => {
                img.style.display = "none";
                const fallback = document.createElement("div");
                fallback.className = "report-fallback";
                fallback.textContent = "Imagen generada en: " + ruta;
                item.appendChild(fallback);
            };
            item.appendChild(img);

        } else if (ext === "txt") {
            // Texto: mostrar contenido
            fetch(`http://localhost:8080/reporte?path=${encodeURIComponent(ruta)}&t=${Date.now()}`)
                .then(r => r.text())
                .then(txt => {
                    const pre = document.createElement("pre");
                    pre.className = "report-txt";
                    pre.textContent = txt;
                    item.appendChild(pre);
                })
                .catch(() => {
                    const fallback = document.createElement("div");
                    fallback.className = "report-fallback";
                    fallback.textContent = "Archivo generado en: " + ruta;
                    item.appendChild(fallback);
                });
        }

        reportsBody.appendChild(item);
    });
}

function limpiarReportes() {
    const reportsBody = document.getElementById("reportsBody");
    reportsBody.innerHTML = `
        <div class="reports-empty" id="reportsEmpty">
            <span class="reports-empty-icon">◫</span>
            <span>Los reportes generados con <code>rep</code> aparecerán aquí</span>
        </div>`;
}

  // Números de línea

function sincronizarScroll() {
    const editor      = document.getElementById("editor");
    const lineNumbers = document.getElementById("lineNumbers");
    lineNumbers.scrollTop = editor.scrollTop;
}

function actualizarLineas() {
    const editor      = document.getElementById("editor");
    const lineNumbers = document.getElementById("lineNumbers");
    const totalLineas = editor.value.split("\n").length;
    let numeros = "";
    for (let i = 1; i <= totalLineas; i++) numeros += i + "\n";
    lineNumbers.textContent = numeros;
}

  // Helper de descarga

function descargarTexto(nombre, contenido) {
    const blob   = new Blob([contenido], { type: "text/plain" });
    const url    = URL.createObjectURL(blob);
    const enlace = document.createElement("a");
    enlace.href     = url;
    enlace.download = nombre;
    enlace.click();
    URL.revokeObjectURL(url);
}

   Init
window.onload = actualizarLineas;