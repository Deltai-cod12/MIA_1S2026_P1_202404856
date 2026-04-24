// ── UTILIDADES DE CONSOLA ──────────────────────────────────────────

function escribirConsola(texto, tipo = "normal") {
    const consola = document.getElementById("console");
    if (!consola) return;

    const linea = document.createElement("div");
    linea.className = "console-line console-" + tipo;
    linea.textContent = texto;

    consola.appendChild(linea);
    consola.scrollTop = consola.scrollHeight;

    // Guardar el estado de la consola para persistencia
    localStorage.setItem("mia_console", consola.innerHTML);
}

function limpiarConsola() {
    const consola = document.getElementById("console");
    if (consola) {
        consola.innerHTML = "";
        localStorage.removeItem("mia_console"); // También limpiar persistencia
    }
}

// ── EDITOR Y ARCHIVOS ─────────────────────────────────────────────

function nuevo() {
    document.getElementById("editor").value = "";
    localStorage.removeItem("mia_editor"); // Limpiar persistencia del editor
    limpiarConsola();
    actualizarLineas();
}

function cargarArchivo(event) {
    const file = event.target.files[0];
    if (!file) return;
    const reader = new FileReader();
    reader.onload = function(e) {
        const contenido = e.target.result;
        document.getElementById("editor").value = contenido;
        localStorage.setItem("mia_editor", contenido); // Guardar al cargar
        actualizarLineas();
    };
    reader.readAsText(file);
    event.target.value = "";
}

function guardarCodigo() {
    const contenido = document.getElementById("editor").value;
    descargarTexto("script.smia", contenido);
}

// ── EJECUCIÓN ─────────────────────────────────────────────────────

function ejecutar() {
    limpiarConsola();
    escribirConsola("Enviando comandos al servidor...", "muted");

    const codigo = document.getElementById("editor").value.trim();

    if (!codigo) {
        escribirConsola("No hay comandos para ejecutar.", "error");
        return;
    }

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

        window.ultimaSalida = salida;
        detectarYMostrarReportes(salida);
    })
    .catch(err => {
        escribirConsola("Error de conexión con el backend.", "error");
        escribirConsola(err.toString(), "error");
    })
    .finally(() => {
        btn.disabled = false;
        btn.textContent = "▶ Ejecutar";
    });
}

// ── NÚMEROS DE LÍNEA Y SCROLL ─────────────────────────────────────

function sincronizarScroll() {
    const editor = document.getElementById("editor");
    const lineNumbers = document.getElementById("lineNumbers");
    if (editor && lineNumbers) {
        lineNumbers.scrollTop = editor.scrollTop;
    }
}

function actualizarLineas() {
    const editor = document.getElementById("editor");
    const lineNumbers = document.getElementById("lineNumbers");
    if (!editor || !lineNumbers) return;

    const totalLineas = editor.value.split("\n").length;
    let numeros = "";
    for (let i = 1; i <= totalLineas; i++) numeros += i + "\n";
    lineNumbers.textContent = numeros;
}

// ── INICIALIZACIÓN (INIT) ──────────────────────────────────────────

window.onload = () => {
    const editor = document.getElementById("editor");
    const consola = document.getElementById("console");

    // 1. Restaurar Editor
    const savedEditor = localStorage.getItem("mia_editor");
    if (savedEditor && editor) {
        editor.value = savedEditor;
    }

    // 2. Restaurar Consola
    const savedConsole = localStorage.getItem("mia_console");
    if (savedConsole && consola) {
        consola.innerHTML = savedConsole;
        consola.scrollTop = consola.scrollHeight;
    }

    // 3. Sincronizar lineas inicialmente
    actualizarLineas();

    // 4. Event Listener para guardar el editor mientras escribes
    if (editor) {
        editor.addEventListener("input", () => {
            actualizarLineas();
            localStorage.setItem("mia_editor", editor.value);
        });
        
        // Sincronizar scroll si el usuario usa la rueda del mouse
        editor.addEventListener("scroll", sincronizarScroll);
    }
};

// [Funciones descargarTexto y detectarYMostrarReportes se mantienen igual]