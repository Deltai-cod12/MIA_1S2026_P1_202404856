#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include "httplib.h"
#include "parser/CommandParser.h"

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

int main() {
    httplib::Server svr;

    svr.set_default_headers({
        {"Access-Control-Allow-Origin",  "*"},
        {"Access-Control-Allow-Methods", "POST, GET, OPTIONS"},
        {"Access-Control-Allow-Headers", "Content-Type"}
    });

    svr.Options(".*", [](const httplib::Request&, httplib::Response& res) {
        res.status = 204;
    });

    // POST /ejecutar  — Body: {"codigo":"..."}  Resp: {"salida":"..."}
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
                    case 't':  decoded += '\t'; i++; break;
                    case 'r':  decoded += '\r'; i++; break;
                    case '"':  decoded += '"';  i++; break;
                    case '\\': decoded += '\\'; i++; break;
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
                case '\r': esc += "\\r";  break;
                case '\t': esc += "\\t";  break;
                default:   esc += c;      break;
            }
        }
        res.set_content("{\"salida\":\"" + esc + "\"}", "application/json");
    });

    // GET /reporte?path=/ruta/imagen.jpg  — sirve el archivo al frontend
    svr.Get("/reporte", [](const httplib::Request& req, httplib::Response& res) {
        if (!req.has_param("path")) {
            res.status = 400; return;
        }
        std::string filePath = req.get_param_value("path");
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) { res.status = 404; return; }

        std::string content((std::istreambuf_iterator<char>(file)),
                             std::istreambuf_iterator<char>());
        file.close();

        std::string ext;
        size_t dot = filePath.find_last_of('.');
        if (dot != std::string::npos) ext = filePath.substr(dot + 1);

        std::string ct = "application/octet-stream";
        if      (ext == "jpg" || ext == "jpeg") ct = "image/jpeg";
        else if (ext == "png")                  ct = "image/png";
        else if (ext == "pdf")                  ct = "application/pdf";
        else if (ext == "txt")                  ct = "text/plain; charset=utf-8";

        res.set_content(content, ct.c_str());
    });

    // Servir frontend estático
    svr.set_mount_point("/", "../../frontend");

    std::cout << "=== MIA Backend en http://localhost:8080 ===\n";
    std::cout << "Abre http://localhost:8080/index.html en el navegador\n";
    svr.listen("0.0.0.0", 8080);
    return 0;
}