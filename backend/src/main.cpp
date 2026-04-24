#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <algorithm>
#include <ctime>
#include "httplib.h"
#include "parser/CommandParser.h"
#include "commands/journaling.h"

namespace fs = std::filesystem;

// ════════════════════════════════════════════════════════════════
// UTILIDADES GENERALES
// ════════════════════════════════════════════════════════════════

// Lee un char[] con posibles nulos y devuelve std::string limpio
std::string cleanStr(const char* arr, int maxLen) {
    std::string s;
    for (int i = 0; i < maxLen; i++) {
        if (arr[i] == '\0') break;
        s += arr[i];
    }
    // Quitar espacios de cola
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.pop_back();
    return s;
}

// Escapa un string para JSON
std::string escJson(const std::string& s) {
    std::string out;
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;
        }
    }
    return out;
}

// Permisos en formato rwxrwxrwx desde char[3] tipo "664"
std::string permStr(const char perm[3]) {
    std::string out;
    for (int i = 0; i < 3; i++) {
        int v = (perm[i] >= '0' && perm[i] <= '7') ? (perm[i] - '0') : 0;
        out += (v & 4) ? 'r' : '-';
        out += (v & 2) ? 'w' : '-';
        out += (v & 1) ? 'x' : '-';
    }
    return out;
}

// ════════════════════════════════════════════════════════════════
// LECTURA DEL SISTEMA DE ARCHIVOS (EXT2 / EXT3)
// ════════════════════════════════════════════════════════════════

// Navegar un path dentro de una partición y devolver el índice de inodo.
// Retorna -1 si no existe.
int navegarPath(std::fstream& f, const Superblock& sb, const std::string& path) {
    if (path == "/" || path.empty()) return 0;

    std::istringstream iss(path);
    std::string token;
    int cur = 0; // inodo raíz

    while (std::getline(iss, token, '/')) {
        if (token.empty()) continue;

        // Leer inodo actual
        Inode dir;
        f.seekg(sb.s_inode_start + cur * sizeof(Inode), std::ios::beg);
        f.read(reinterpret_cast<char*>(&dir), sizeof(Inode));

        bool found = false;
        // Buscar token en los bloques directos
        for (int b = 0; b < 12 && !found; b++) {
            if (dir.i_block[b] == -1) break;
            FolderBlock fb;
            f.seekg(sb.s_block_start + dir.i_block[b] * sizeof(FolderBlock), std::ios::beg);
            f.read(reinterpret_cast<char*>(&fb), sizeof(FolderBlock));
            for (int j = 0; j < 4 && !found; j++) {
                if (fb.b_content[j].b_inodo == -1) continue;
                // Comparar con 11 chars (igual que mkdir.h)
                if (strncmp(fb.b_content[j].b_name, token.c_str(), 11) == 0) {
                    cur = fb.b_content[j].b_inodo;
                    found = true;
                }
            }
        }
        if (!found) return -1;
    }
    return cur;
}

// Listar el contenido de un directorio dado su inodo.
// Devuelve JSON array de entries.
std::string listarDirectorio(std::fstream& f, const Superblock& sb, int dirInodo) {
    Inode dir;
    f.seekg(sb.s_inode_start + dirInodo * sizeof(Inode), std::ios::beg);
    f.read(reinterpret_cast<char*>(&dir), sizeof(Inode));

    std::string json = "[";
    bool primero = true;

    for (int b = 0; b < 12; b++) {
        if (dir.i_block[b] == -1) break;
        FolderBlock fb;
        f.seekg(sb.s_block_start + dir.i_block[b] * sizeof(FolderBlock), std::ios::beg);
        f.read(reinterpret_cast<char*>(&fb), sizeof(FolderBlock));

        for (int j = 0; j < 4; j++) {
            if (fb.b_content[j].b_inodo == -1) continue;
            std::string nombre = cleanStr(fb.b_content[j].b_name, 12);
            if (nombre == "." || nombre == "..") continue;

            // Leer inodo del hijo
            Inode hijo;
            f.seekg(sb.s_inode_start + fb.b_content[j].b_inodo * sizeof(Inode), std::ios::beg);
            f.read(reinterpret_cast<char*>(&hijo), sizeof(Inode));

            if (!primero) json += ",";
            primero = false;

            std::string tipo   = (hijo.i_type == '1') ? "carpeta" : "archivo";
            std::string perm   = permStr(hijo.i_perm);
            int         tam    = hijo.i_size;
            int         uid    = hijo.i_uid;

            json += "{";
            json += "\"nombre\":\"" + escJson(nombre) + "\",";
            json += "\"tipo\":\""   + tipo             + "\",";
            json += "\"permisos\":\"" + perm           + "\",";
            json += "\"tamano\":"  + std::to_string(tam) + ",";
            json += "\"uid\":"     + std::to_string(uid);
            json += "}";
        }
    }

    json += "]";
    return json;
}

// Leer el contenido completo de un archivo dado su inodo.
std::string leerArchivo(std::fstream& f, const Superblock& sb, int inodo) {
    Inode inode;
    f.seekg(sb.s_inode_start + inodo * sizeof(Inode), std::ios::beg);
    f.read(reinterpret_cast<char*>(&inode), sizeof(Inode));

    std::string content;

    // Bloques directos
    for (int i = 0; i < 12; i++) {
        if (inode.i_block[i] == -1) break;
        FileBlock fb;
        f.seekg(sb.s_block_start + inode.i_block[i] * sizeof(FileBlock), std::ios::beg);
        f.read(reinterpret_cast<char*>(&fb), sizeof(FileBlock));
        int len = strnlen(fb.b_content, sizeof(fb.b_content));
        content.append(fb.b_content, len);
    }

    // Bloque indirecto simple
    if (inode.i_block[12] != -1) {
        PointerBlock pb;
        f.seekg(sb.s_block_start + inode.i_block[12] * sizeof(PointerBlock), std::ios::beg);
        f.read(reinterpret_cast<char*>(&pb), sizeof(PointerBlock));
        for (int i = 0; i < 16; i++) {
            if (pb.b_pointers[i] == -1) break;
            FileBlock fb;
            f.seekg(sb.s_block_start + pb.b_pointers[i] * sizeof(FileBlock), std::ios::beg);
            f.read(reinterpret_cast<char*>(&fb), sizeof(FileBlock));
            int len = strnlen(fb.b_content, sizeof(fb.b_content));
            content.append(fb.b_content, len);
        }
    }

    if (inode.i_size > 0 && (int)content.size() > inode.i_size)
        content.resize(inode.i_size);

    return content;
}

// ════════════════════════════════════════════════════════════════
// EJECUCIÓN DE SCRIPTS
// ════════════════════════════════════════════════════════════════

std::string ejecutarScript(const std::string& script) {
    std::string output;
    std::istringstream ss(script);
    std::string line;

    while (std::getline(ss, line)) {
        if (line.empty() || line.find_first_not_of(" \t\r\n") == std::string::npos)
            continue;
        if (line[0] == '#') { output += line + "\n"; continue; }

        output += ">> " + line + "\n";
        std::string result = CommandParser::execute(line);
        if (result == "EXIT") { output += "Sistema terminado.\n"; break; }
        if (!result.empty())  output += result + "\n";
    }
    return output;
}

// ════════════════════════════════════════════════════════════════
// MAIN — SERVIDOR HTTP
// ════════════════════════════════════════════════════════════════

int main() {
    httplib::Server svr;

    // CORS
    svr.set_default_headers({
        {"Access-Control-Allow-Origin",  "*"},
        {"Access-Control-Allow-Methods", "POST, GET, OPTIONS"},
        {"Access-Control-Allow-Headers", "Content-Type"}
    });
    svr.Options(".*", [](const httplib::Request&, httplib::Response& res) {
        res.status = 204;
    });

    // ────────────────────────────────────────────────────────
    // POST /ejecutar — ejecutar script de comandos
    // ────────────────────────────────────────────────────────
    svr.Post("/ejecutar", [](const httplib::Request& req, httplib::Response& res) {
        std::string body = req.body;
        std::string codigo;

        size_t pos = body.find("\"codigo\"");
        if (pos != std::string::npos) {
            pos = body.find(":", pos);
            pos = body.find("\"", pos);
            if (pos != std::string::npos) {
                size_t end = pos + 1;
                while (end < body.size()) {
                    if (body[end] == '\\') { end += 2; continue; }
                    if (body[end] == '"')  { break; }
                    end++;
                }
                codigo = body.substr(pos + 1, end - pos - 1);
            }
        }

        // Decodificar escapes JSON
        std::string decoded;
        for (size_t i = 0; i < codigo.size(); i++) {
            if (codigo[i] == '\\' && i + 1 < codigo.size()) {
                switch (codigo[i+1]) {
                    case 'n':  decoded += '\n'; i++; break;
                    case '"':  decoded += '"';  i++; break;
                    case '\\': decoded += '\\'; i++; break;
                    case 't':  decoded += '\t'; i++; break;
                    default:   decoded += codigo[i]; break;
                }
            } else { decoded += codigo[i]; }
        }

        std::string salida = ejecutarScript(decoded);

        // Escapar para JSON
        std::string esc;
        for (char c : salida) {
            switch (c) {
                case '"':  esc += "\\\""; break;
                case '\\': esc += "\\\\"; break;
                case '\n': esc += "\\n";  break;
                case '\t': esc += "\\t";  break;
                default:   esc += c;
            }
        }
        res.set_content("{\"salida\":\"" + esc + "\"}", "application/json");
    });

    // ────────────────────────────────────────────────────────
    // GET /sesion — estado de la sesión activa
    // ────────────────────────────────────────────────────────
    svr.Get("/sesion", [](const httplib::Request&, httplib::Response& res) {
        bool activa = currentSession.active;
        std::string json = "{";
        json += "\"active\":"  + std::string(activa ? "true" : "false") + ",";
        json += "\"user\":\""  + escJson(currentSession.user)   + "\",";
        json += "\"group\":\"" + escJson(currentSession.group)  + "\",";
        json += "\"id\":\""    + escJson(currentSession.id)     + "\"";
        json += "}";
        res.set_content(json, "application/json");
    });

    // ────────────────────────────────────────────────────────
    // GET /discos?path=/ruta — listar archivos y carpetas
    // Devuelve array de items (archivos .mia y carpetas)
    // ────────────────────────────────────────────────────────
    svr.Get("/discos", [](const httplib::Request& req, httplib::Response& res) {
        std::string pathBusqueda = req.has_param("path")
            ? req.get_param_value("path")
            : "/home";

        // Expandir ~ si aparece
        if (!pathBusqueda.empty() && pathBusqueda[0] == '~') {
            const char* home = std::getenv("HOME");
            if (home) pathBusqueda = std::string(home) + pathBusqueda.substr(1);
        }

        std::string json = "[";
        bool primero = true;

        try {
            if (!fs::exists(pathBusqueda) || !fs::is_directory(pathBusqueda)) {
                res.set_content("[]", "application/json");
                return;
            }

            // Recolectar y ordenar (carpetas primero, luego discos)
            std::vector<fs::directory_entry> entries;
            for (const auto& e : fs::directory_iterator(pathBusqueda))
                entries.push_back(e);
            std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
                bool aDir = fs::is_directory(a.path());
                bool bDir = fs::is_directory(b.path());
                if (aDir != bDir) return aDir > bDir;
                return a.path().filename() < b.path().filename();
            });

            for (const auto& entry : entries) {
                bool isDir = fs::is_directory(entry.path());
                std::string ext = entry.path().extension().string();
                bool isDisco = !isDir && (ext == ".mia" || ext == ".dsk");

                // Mostrar solo carpetas y discos
                if (!isDir && !isDisco) continue;

                if (!primero) json += ",";
                primero = false;

                std::string nombre = entry.path().filename().string();
                std::string ruta   = entry.path().string();
                long tamano = 0;
                std::string fit = "—";

                if (isDisco) {
                    tamano = static_cast<long>(fs::file_size(entry.path()));
                    // Leer fit del MBR
                    std::ifstream f(ruta, std::ios::binary);
                    if (f.is_open()) {
                        MBR mbr;
                        f.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));
                        if (mbr.dsk_fit == 'B') fit = "BF";
                        else if (mbr.dsk_fit == 'F') fit = "FF";
                        else if (mbr.dsk_fit == 'W') fit = "WF";
                        f.close();
                    }
                }

                json += "{";
                json += "\"nombre\":\""     + escJson(nombre)          + "\",";
                json += "\"ruta\":\""       + escJson(ruta)            + "\",";
                json += "\"esDirectorio\":" + std::string(isDir ? "true" : "false") + ",";
                json += "\"tamano\":"       + std::to_string(tamano)   + ",";
                json += "\"fit\":\""        + fit                      + "\",";
                json += "\"particiones\":[]"; // vacío; /particiones lo detalla
                json += "}";
            }
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content("{\"error\":\"" + escJson(e.what()) + "\"}", "application/json");
            return;
        }

        json += "]";
        res.set_content(json, "application/json");
    });

    // ────────────────────────────────────────────────────────
    // GET /particiones?disco=/ruta/disco.mia
    // Lee MBR + EBRs y combina con particiones montadas
    // ────────────────────────────────────────────────────────
    svr.Get("/particiones", [](const httplib::Request& req, httplib::Response& res) {
        if (!req.has_param("disco")) {
            res.set_content("{\"error\":\"Falta parametro disco\"}", "application/json");
            res.status = 400; return;
        }

        std::string discoPath = req.get_param_value("disco");
        // Expandir ~
        if (!discoPath.empty() && discoPath[0] == '~') {
            const char* home = std::getenv("HOME");
            if (home) discoPath = std::string(home) + discoPath.substr(1);
        }

        std::ifstream file(discoPath, std::ios::binary);
        if (!file.is_open()) {
            res.set_content("{\"error\":\"No se pudo abrir el disco\"}", "application/json");
            res.status = 404; return;
        }

        MBR mbr;
        file.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));

        // Función interna: buscar ID de montaje para un nombre y path dados
        auto buscarId = [&](const std::string& nombre) -> std::string {
            for (auto const& [id, m] : CommandMount::mountedPartitions) {
                // Comparar rutas normalizando (sin preocuparse por ~)
                std::string mp = m.path;
                if (!mp.empty() && mp[0] == '~') {
                    const char* home = std::getenv("HOME");
                    if (home) mp = std::string(home) + mp.substr(1);
                }
                // Comparar nombre de partición (strncmp 11 chars como en mkdir)
                if ((mp == discoPath || (fs::exists(mp) && fs::exists(discoPath) &&
                     fs::equivalent(mp, discoPath)))
                    && strncmp(m.name.c_str(), nombre.c_str(), 11) == 0) {
                    return id;
                }
            }
            return "";
        };

        // Función interna: leer superbloque de una partición y obtener tipo de FS
        auto leerFS = [&](int start, int size) -> std::string {
            if (start <= 0 || size <= 0) return "Sin formato";
            file.seekg(start, std::ios::beg);
            Superblock sb;
            file.read(reinterpret_cast<char*>(&sb), sizeof(Superblock));
            if (sb.s_magic == 0xEF53) {
                return "EXT" + std::to_string(sb.s_filesystem_type);
            }
            return "Sin formato";
        };

        std::string json = "{\"particiones\":[";
        bool primero = true;

        for (int i = 0; i < 4; i++) {
            Partition& p = mbr.mbr_partitions[i];
            if (p.part_status != '1' || p.part_s <= 0) continue;

            std::string nombre = cleanStr(p.part_name, 16);
            std::string id     = buscarId(nombre);
            std::string fsType = leerFS(p.part_start, p.part_s);
            char tipo = p.part_type; // 'P','E','L'

            if (!primero) json += ",";
            primero = false;

            json += "{";
            json += "\"nombre\":\""     + escJson(nombre)          + "\",";
            json += "\"id\":\""         + escJson(id.empty() ? "SIN ID" : id) + "\",";
            json += "\"tamano\":"       + std::to_string(p.part_s) + ",";
            json += "\"tipo\":\""       + std::string(1, tipo)     + "\",";
            json += "\"fit\":\""        + std::string(1, p.part_fit) + "\",";
            json += "\"filesystem\":\"" + fsType                   + "\"";
            json += "}";

            // Si es extendida, también listar las lógicas via EBRs
            if (tipo == 'E') {
                int ebrPos = p.part_start;
                while (ebrPos != -1 && ebrPos > 0) {
                    file.seekg(ebrPos, std::ios::beg);
                    EBR ebr;
                    file.read(reinterpret_cast<char*>(&ebr), sizeof(EBR));

                    if (ebr.part_status == '1' && ebr.part_size > 0) {
                        std::string eNombre = cleanStr(ebr.part_name, 16);
                        std::string eId     = buscarId(eNombre);
                        std::string eFs     = leerFS(ebr.part_start, ebr.part_size);

                        json += ",{";
                        json += "\"nombre\":\""     + escJson(eNombre)             + "\",";
                        json += "\"id\":\""         + escJson(eId.empty() ? "SIN ID" : eId) + "\",";
                        json += "\"tamano\":"       + std::to_string(ebr.part_size) + ",";
                        json += "\"tipo\":\"L\",";
                        json += "\"fit\":\""        + std::string(1, ebr.part_fit) + "\",";
                        json += "\"filesystem\":\"" + eFs                          + "\"";
                        json += "}";
                    }

                    ebrPos = ebr.part_next;
                }
            }
        }

        file.close();
        json += "]}";
        res.set_content(json, "application/json");
    });

    // ────────────────────────────────────────────────────────
    // GET /archivos?id=XXX&path=/ruta
    // Lista el contenido de un directorio en el FS virtual
    // ────────────────────────────────────────────────────────
    svr.Get("/archivos", [](const httplib::Request& req, httplib::Response& res) {
        if (!req.has_param("id") || !req.has_param("path")) {
            res.set_content("{\"error\":\"Faltan parametros id o path\"}", "application/json");
            res.status = 400; return;
        }

        std::string id   = req.get_param_value("id");
        std::string path = req.get_param_value("path");

        CommandMount::MountedPartition part;
        if (!CommandMount::getMountedPartition(id, part)) {
            res.set_content("{\"error\":\"Particion no montada: " + escJson(id) + "\"}", "application/json");
            res.status = 404; return;
        }

        std::fstream file(part.path, std::ios::binary | std::ios::in);
        if (!file.is_open()) {
            res.set_content("{\"error\":\"No se pudo abrir el disco\"}", "application/json");
            res.status = 500; return;
        }

        Superblock sb;
        file.seekg(part.start, std::ios::beg);
        file.read(reinterpret_cast<char*>(&sb), sizeof(Superblock));

        if (sb.s_magic != 0xEF53) {
            file.close();
            res.set_content("{\"error\":\"La particion no tiene sistema de archivos. Usa mkfs primero.\"}", "application/json");
            res.status = 422; return;
        }

        // Navegar al directorio pedido
        int dirInodo = navegarPath(file, sb, path);
        if (dirInodo == -1) {
            file.close();
            res.set_content("{\"error\":\"Directorio no encontrado: " + escJson(path) + "\"}", "application/json");
            res.status = 404; return;
        }

        // Verificar que es carpeta
        Inode dirNode;
        file.seekg(sb.s_inode_start + dirInodo * sizeof(Inode), std::ios::beg);
        file.read(reinterpret_cast<char*>(&dirNode), sizeof(Inode));
        if (dirNode.i_type != '1') {
            file.close();
            res.set_content("{\"error\":\"La ruta no es un directorio\"}", "application/json");
            res.status = 422; return;
        }

        std::string archivosJson = listarDirectorio(file, sb, dirInodo);
        file.close();

        res.set_content("{\"archivos\":" + archivosJson + "}", "application/json");
    });

    // ────────────────────────────────────────────────────────
    // GET /archivo?id=XXX&path=/ruta/archivo.txt
    // Devuelve el contenido de un archivo
    // ────────────────────────────────────────────────────────
    svr.Get("/archivo", [](const httplib::Request& req, httplib::Response& res) {
        if (!req.has_param("id") || !req.has_param("path")) {
            res.set_content("{\"error\":\"Faltan parametros\"}", "application/json");
            res.status = 400; return;
        }

        std::string id   = req.get_param_value("id");
        std::string path = req.get_param_value("path");

        CommandMount::MountedPartition part;
        if (!CommandMount::getMountedPartition(id, part)) {
            res.set_content("{\"error\":\"Particion no montada\"}", "application/json");
            res.status = 404; return;
        }

        std::fstream file(part.path, std::ios::binary | std::ios::in);
        if (!file.is_open()) {
            res.set_content("{\"error\":\"No se pudo abrir el disco\"}", "application/json");
            res.status = 500; return;
        }

        Superblock sb;
        file.seekg(part.start, std::ios::beg);
        file.read(reinterpret_cast<char*>(&sb), sizeof(Superblock));

        if (sb.s_magic != 0xEF53) {
            file.close();
            res.set_content("{\"error\":\"Sin sistema de archivos\"}", "application/json");
            res.status = 422; return;
        }

        int inodo = navegarPath(file, sb, path);
        if (inodo == -1) {
            file.close();
            res.set_content("{\"error\":\"Archivo no encontrado\"}", "application/json");
            res.status = 404; return;
        }

        // Verificar que es archivo
        Inode node;
        file.seekg(sb.s_inode_start + inodo * sizeof(Inode), std::ios::beg);
        file.read(reinterpret_cast<char*>(&node), sizeof(Inode));
        if (node.i_type == '1') {
            file.close();
            res.set_content("{\"error\":\"La ruta es un directorio, no un archivo\"}", "application/json");
            res.status = 422; return;
        }

        std::string contenido = leerArchivo(file, sb, inodo);
        file.close();

        res.set_content("{\"contenido\":\"" + escJson(contenido) + "\"}", "application/json");
    });

    // ────────────────────────────────────────────────────────
    // GET /journaling?id=XXX
    // Devuelve las entradas del journal de una partición EXT3
    // ────────────────────────────────────────────────────────
    svr.Get("/journaling", [](const httplib::Request& req, httplib::Response& res) {
        if (!req.has_param("id")) {
            res.set_content("{\"error\":\"Falta parametro id\"}", "application/json");
            res.status = 400; return;
        }

        std::string id = req.get_param_value("id");
        CommandMount::MountedPartition part;
        if (!CommandMount::getMountedPartition(id, part)) {
            res.set_content("{\"error\":\"Particion no montada: " + escJson(id) + "\"}", "application/json");
            res.status = 404; return;
        }

        std::fstream file(part.path, std::ios::binary | std::ios::in);
        if (!file.is_open()) {
            res.set_content("{\"error\":\"No se pudo abrir disco\"}", "application/json");
            res.status = 500; return;
        }

        Superblock sb;
        file.seekg(part.start, std::ios::beg);
        file.read(reinterpret_cast<char*>(&sb), sizeof(Superblock));

        if (sb.s_filesystem_type != 3) {
            file.close();
            res.set_content("{\"entries\":[],\"error\":\"Solo EXT3 tiene journaling\"}", "application/json");
            return;
        }

        auto entries = CommandJournaling::readFromDisk(file, sb);
        file.close();

        std::string json = "{\"entries\":[";
        bool primero = true;
        for (const auto& j : entries) {
            if (!primero) json += ",";
            primero = false;
            std::time_t t = static_cast<std::time_t>(j.j_content.i_date);
            std::string op   = cleanStr(j.j_content.i_operation, 10);
            std::string path = cleanStr(j.j_content.i_path, 32);
            std::string cont = cleanStr(j.j_content.i_content, 64);
            std::string fecha= CommandJournaling::dateTime(t);

            json += "{";
            json += "\"operacion\":\"" + escJson(op)    + "\",";
            json += "\"path\":\""      + escJson(path)  + "\",";
            json += "\"contenido\":\"" + escJson(cont)  + "\",";
            json += "\"fecha\":\""     + escJson(fecha) + "\"";
            json += "}";
        }
        json += "]}";
        res.set_content(json, "application/json");
    });

    // ────────────────────────────────────────────────────────
    // GET /particiones-montadas — todas las particiones montadas
    // Útil para poblar el select de journaling
    // ────────────────────────────────────────────────────────
    svr.Get("/particiones-montadas", [](const httplib::Request&, httplib::Response& res) {
        std::string json = "[";
        bool primero = true;
        for (const auto& [id, m] : CommandMount::mountedPartitions) {
            if (!primero) json += ",";
            primero = false;
            // Leer tipo de FS
            std::string fsType = "—";
            std::ifstream f(m.path, std::ios::binary);
            if (f.is_open()) {
                f.seekg(m.start, std::ios::beg);
                Superblock sb;
                f.read(reinterpret_cast<char*>(&sb), sizeof(Superblock));
                if (sb.s_magic == 0xEF53)
                    fsType = "EXT" + std::to_string(sb.s_filesystem_type);
                f.close();
            }
            json += "{";
            json += "\"id\":\""     + escJson(id)    + "\",";
            json += "\"nombre\":\"" + escJson(m.name) + "\",";
            json += "\"disco\":\""  + escJson(m.path) + "\",";
            json += "\"fs\":\""     + fsType          + "\"";
            json += "}";
        }
        json += "]";
        res.set_content(json, "application/json");
    });

    // ────────────────────────────────────────────────────────
    // GET /reporte?path=/ruta — servir archivo de reporte
    // ────────────────────────────────────────────────────────
    svr.Get("/reporte", [](const httplib::Request& req, httplib::Response& res) {
        if (!req.has_param("path")) { res.status = 400; return; }
        std::string filePath = req.get_param_value("path");
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) { res.status = 404; return; }
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        std::string ext = fs::path(filePath).extension().string();
        std::string mime = "application/octet-stream";
        if (ext == ".png")  mime = "image/png";
        else if (ext == ".jpg" || ext == ".jpeg") mime = "image/jpeg";
        else if (ext == ".txt") mime = "text/plain";
        res.set_content(content, mime);
    });

    // Servir el frontend estático
    svr.set_mount_point("/", "../../frontend");

    std::cout << "=== C++Disk 2.0 Backend en http://localhost:8080 ===" << std::endl;
    std::cout << "Abre http://localhost:8080/index.html en el navegador" << std::endl;
    svr.listen("0.0.0.0", 8080);
    return 0;
}