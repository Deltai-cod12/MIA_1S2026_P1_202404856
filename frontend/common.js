// common.js - Funciones compartidas entre páginas

function getLocalSession() {
    const s = localStorage.getItem("mia_session");
    return s ? JSON.parse(s) : null;
}

async function checkSession() {
    const s = getLocalSession();
    if (!s || !s.active) return null;
    return s;
}