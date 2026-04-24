#ifndef JOURNALING_H
#define JOURNALING_H

/**
 * journaling.h
 * Gestiona el journaling EXT3.
 */

#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <cstring>
#include <fstream>
#include <algorithm>
#include "../models/structs.h"
#include "mount.h"
#include "../session/session.h"

namespace CommandJournaling {

    // Entrada en memoria (para consulta rápida)
    struct Entry {
        std::string mountId;
        std::string operation;
        std::string path;
        std::string content;
        std::time_t when;
    };

    inline std::vector<Entry>& store() {
        static std::vector<Entry> entries;
        return entries;
    }

    // Helpers de formato
    inline std::string normalize(const std::string& s) {
        std::string out = s;
        for (char& c : out)
            if (c == '\n' || c == '\r' || c == '\t') c = ' ';
        return out;
    }

    inline std::string fitStr(const std::string& s, size_t width) {
        std::string clean = normalize(s);
        if (clean.size() <= width) return clean + std::string(width - clean.size(), ' ');
        if (width <= 3) return clean.substr(0, width);
        return clean.substr(0, width - 3) + "...";
    }

    inline std::string dateTime(std::time_t t) {
        char buf[32];
        std::tm* ti = std::localtime(&t);
        std::strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M", ti);
        return std::string(buf);
    }

    inline std::string toLower(const std::string& s) {
        std::string o = s;
        std::transform(o.begin(), o.end(), o.begin(), ::tolower);
        return o;
    }

    // Escritura en disco (solo EXT3)
    inline void writeToDisk(std::fstream& file, Superblock& sb, int partStart,
                             const std::string& operation,
                             const std::string& path,
                             const std::string& content) {
        if (sb.s_filesystem_type != 3) return;
        if (sb.s_journal_start == 0)   return;
        if (sb.s_journal_count >= JOURNAL_MAX) return;

        Journal j;
        j.j_count = sb.s_journal_count;

        strncpy(j.j_content.i_operation, operation.c_str(), 9);
        j.j_content.i_operation[9] = '\0';

        strncpy(j.j_content.i_path, path.c_str(), 31);
        j.j_content.i_path[31] = '\0';

        std::string cont = content.empty() ? "-" : content;
        strncpy(j.j_content.i_content, cont.c_str(), 63);
        j.j_content.i_content[63] = '\0';

        j.j_content.i_date = static_cast<float>(std::time(nullptr));

        int offset = sb.s_journal_start +
                     sb.s_journal_count * static_cast<int>(sizeof(Journal));
        file.seekp(offset, std::ios::beg);
        file.write(reinterpret_cast<const char*>(&j), sizeof(Journal));

        // Actualizar contador en superbloque
        sb.s_journal_count++;
        file.seekp(partStart, std::ios::beg);
        file.write(reinterpret_cast<const char*>(&sb), sizeof(Superblock));
        file.flush();
    }

    // Registro en memoria
    inline void add(const std::string& mountId,
                    const std::string& operation,
                    const std::string& path,
                    const std::string& content) {
        if (mountId.empty()) return;
        store().push_back({mountId, operation, path, content, std::time(nullptr)});
    }

    // Limpiar entradas de una partición (al formatear con mkfs)
    inline void clearFor(const std::string& mountId) {
        if (mountId.empty()) return;
        auto& entries = store();
        std::string target = toLower(mountId);
        entries.erase(
            std::remove_if(entries.begin(), entries.end(), [&](const Entry& e) {
                return toLower(e.mountId) == target;
            }),
            entries.end()
        );
    }

    // Leer entradas desde disco (EXT3)
    inline std::vector<Journal> readFromDisk(std::fstream& file, const Superblock& sb) {
        std::vector<Journal> result;
        if (sb.s_filesystem_type != 3 || sb.s_journal_start == 0) return result;
        int count = (sb.s_journal_count > JOURNAL_MAX) ? JOURNAL_MAX : sb.s_journal_count;
        for (int i = 0; i < count; i++) {
            Journal j;
            int offset = sb.s_journal_start + i * static_cast<int>(sizeof(Journal));
            file.seekg(offset, std::ios::beg);
            file.read(reinterpret_cast<char*>(&j), sizeof(Journal));
            result.push_back(j);
        }
        return result;
    }

    // Comando: journaling -id=<id>
    inline std::string parseParam(const std::string& line, const std::string& param) {
        std::string ll = toLower(line);
        std::string pp = toLower(param);
        size_t pos = ll.find(pp + "=");
        if (pos == std::string::npos) return "";
        size_t start = pos + param.size() + 1;
        if (start >= line.size()) return "";
        if (line[start] == '"' || line[start] == '\'') {
            char q = line[start];
            size_t end = line.find(q, start + 1);
            return line.substr(start + 1,
                (end == std::string::npos ? line.size() : end) - start - 1);
        }
        size_t end = line.find(' ', start);
        return line.substr(start, (end == std::string::npos ? line.size() : end) - start);
    }

    inline std::string executeFromLine(const std::string& commandLine) {
        std::string id = parseParam(commandLine, "-id");
        if (id.empty()) return "Error: journaling requiere -id";

        // Intentar abrir el disco para leer desde disco si es EXT3
        CommandMount::MountedPartition part;
        std::ostringstream out;
        out << "\n=== JOURNALING ===\n";
        out << "Partición: " << id << "\n";

        if (CommandMount::getMountedPartition(id, part)) {
            std::fstream file(part.path, std::ios::binary | std::ios::in);
            if (file.is_open()) {
                Superblock sb;
                file.seekg(part.start, std::ios::beg);
                file.read(reinterpret_cast<char*>(&sb), sizeof(Superblock));

                if (sb.s_filesystem_type == 3) {
                    // Leer desde disco
                    auto entries = readFromDisk(file, sb);
                    file.close();

                    if (entries.empty()) {
                        out << "Sin transacciones registradas (EXT3 - disco).\n";
                        return out.str();
                    }

                    const size_t wOp=12, wPath=30, wCont=30, wDate=18;
                    auto sep = [&]() {
                        out << "+" << std::string(wOp+2,'-')
                            << "+" << std::string(wPath+2,'-')
                            << "+" << std::string(wCont+2,'-')
                            << "+" << std::string(wDate+2,'-') << "+\n";
                    };
                    sep();
                    out << "| " << fitStr("Operacion",wOp)
                        << " | " << fitStr("Path",wPath)
                        << " | " << fitStr("Contenido",wCont)
                        << " | " << fitStr("Fecha",wDate) << " |\n";
                    sep();
                    for (const auto& j : entries) {
                        std::time_t t = static_cast<std::time_t>(j.j_content.i_date);
                        out << "| " << fitStr(std::string(j.j_content.i_operation), wOp)
                            << " | " << fitStr(std::string(j.j_content.i_path), wPath)
                            << " | " << fitStr(std::string(j.j_content.i_content), wCont)
                            << " | " << fitStr(dateTime(t), wDate) << " |\n";
                    }
                    sep();
                    out << "Total: " << entries.size() << " transaccion(es) [EXT3 - desde disco]\n";
                    return out.str();
                }
                file.close();
            }
        }

        // Fallback: memoria (EXT2 o sin disco disponible)
        std::string target = toLower(id);
        std::vector<Entry> filtered;
        for (const auto& e : store())
            if (toLower(e.mountId) == target) filtered.push_back(e);

        if (filtered.empty()) {
            out << "Sin transacciones registradas.\n";
            return out.str();
        }

        const size_t wOp=12, wPath=30, wCont=30, wDate=18;
        auto sep2 = [&]() {
            out << "+" << std::string(wOp+2,'-')
                << "+" << std::string(wPath+2,'-')
                << "+" << std::string(wCont+2,'-')
                << "+" << std::string(wDate+2,'-') << "+\n";
        };
        sep2();
        out << "| " << fitStr("Operacion",wOp)
            << " | " << fitStr("Path",wPath)
            << " | " << fitStr("Contenido",wCont)
            << " | " << fitStr("Fecha",wDate) << " |\n";
        sep2();
        for (const auto& e : filtered) {
            out << "| " << fitStr(e.operation,wOp)
                << " | " << fitStr(e.path,wPath)
                << " | " << fitStr(e.content,wCont)
                << " | " << fitStr(dateTime(e.when),wDate) << " |\n";
        }
        sep2();
        out << "Total: " << filtered.size() << " transaccion(es)\n";
        return out.str();
    }

} // namespace CommandJournaling

#endif // JOURNALING_H