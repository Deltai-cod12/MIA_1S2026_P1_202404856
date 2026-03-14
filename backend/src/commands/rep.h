#ifndef REP_H
#define REP_H

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <algorithm>
#include <vector>
#include <cstring>
#include <iomanip>
#include <cstdlib>
#include <ctime>
#include <sys/stat.h>
#include <libgen.h>

#include "../models/structs.h"
#include "../commands/mount.h"

namespace CommandRep {

// ─── Utilidades ───────────────────────────────────────────────────────────────

inline std::string toLowerCase(const std::string& str) {
    std::string r = str;
    std::transform(r.begin(), r.end(), r.begin(), ::tolower);
    return r;
}

inline void createDirectories(const std::string& path) {
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s", path.c_str());
    for (char* p = tmp + 1; *p; p++) {
        if (*p == '/') { *p = 0; mkdir(tmp, S_IRWXU); *p = '/'; }
    }
    mkdir(tmp, S_IRWXU);
}

inline std::string getParentPath(const std::string& path) {
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s", path.c_str());
    return std::string(dirname(tmp));
}

inline std::string getExtension(const std::string& path) {
    size_t pos = path.find_last_of('.');
    if (pos == std::string::npos) return "jpg";
    return toLowerCase(path.substr(pos + 1));
}

inline std::string cleanStr(const char* buf, int maxLen) {
    std::string s;
    for (int i = 0; i < maxLen && buf[i] != '\0'; i++) s += buf[i];
    return s;
}

inline std::string fmtTime(time_t t) {
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", localtime(&t));
    return std::string(buf);
}

inline std::string renderDot(const std::string& dotContent, const std::string& outputPath) {
    createDirectories(getParentPath(outputPath));
    std::string dotPath = outputPath + ".dot";
    std::ofstream f(dotPath);
    if (!f.is_open()) return "Error: no se pudo escribir .dot";
    f << dotContent; f.close();
    std::string ext = getExtension(outputPath);
    std::string cmd = "dot -T" + ext + " \"" + dotPath + "\" -o \"" + outputPath + "\" 2>/dev/null";
    int ret = system(cmd.c_str());
    remove(dotPath.c_str());
    if (ret != 0) return "Error: Graphviz fallo. Verifica que 'dot' este instalado.";
    return "";
}

// Helper: leer contenido completo de un inodo
inline std::string readInodeContent(std::fstream& file, Superblock& sb, Inode& inode) {
    std::string content;
    int remaining = inode.i_size;
    for (int b = 0; b < 12 && remaining > 0; b++) {
        if (inode.i_block[b] == -1) continue;
        FileBlock fb;
        file.seekg(sb.s_block_start + inode.i_block[b] * (int)sizeof(FileBlock));
        file.read((char*)&fb, sizeof(FileBlock));
        int chunk = std::min(64, remaining);
        content.append(fb.b_content, chunk);
        remaining -= chunk;
    }
    if (inode.i_block[12] != -1 && remaining > 0) {
        PointerBlock pb;
        file.seekg(sb.s_block_start + inode.i_block[12] * (int)sizeof(PointerBlock));
        file.read((char*)&pb, sizeof(PointerBlock));
        for (int i = 0; i < 16 && remaining > 0; i++) {
            if (pb.b_pointers[i] == -1) continue;
            FileBlock fb;
            file.seekg(sb.s_block_start + pb.b_pointers[i] * (int)sizeof(FileBlock));
            file.read((char*)&fb, sizeof(FileBlock));
            int chunk = std::min(64, remaining);
            content.append(fb.b_content, chunk);
            remaining -= chunk;
        }
    }
    if (inode.i_block[13] != -1 && remaining > 0) {
        PointerBlock outer;
        file.seekg(sb.s_block_start + inode.i_block[13] * (int)sizeof(PointerBlock));
        file.read((char*)&outer, sizeof(PointerBlock));
        for (int i = 0; i < 16 && remaining > 0; i++) {
            if (outer.b_pointers[i] == -1) continue;
            PointerBlock inner;
            file.seekg(sb.s_block_start + outer.b_pointers[i] * (int)sizeof(PointerBlock));
            file.read((char*)&inner, sizeof(PointerBlock));
            for (int j = 0; j < 16 && remaining > 0; j++) {
                if (inner.b_pointers[j] == -1) continue;
                FileBlock fb;
                file.seekg(sb.s_block_start + inner.b_pointers[j] * (int)sizeof(FileBlock));
                file.read((char*)&fb, sizeof(FileBlock));
                int chunk = std::min(64, remaining);
                content.append(fb.b_content, chunk);
                remaining -= chunk;
            }
        }
    }
    return content;
}
// ─── Forward declaration (renderPointerBlock llama a buildTree) ───────────────
inline void buildTree(std::fstream& file, Superblock& sb,
                       int inodeIdx, const std::string& label,
                       std::ostringstream& dot,
                       std::vector<int>& visitedInodes,
                       std::vector<int>& visitedBlocks);

inline void renderPointerBlock(std::fstream& file, Superblock& sb,
                                 int blockIdx, PointerBlock& pb, char inodeType,
                                 std::ostringstream& dot,
                                 std::vector<int>& visitedInodes,
                                 std::vector<int>& visitedBlocks,
                                 int level) {
    // Nodo del bloque de apuntadores (ROSA)
    dot << "  bloque" << blockIdx << " [label=<\n";
    dot << "    <TABLE BORDER=\"1\" CELLBORDER=\"0\" CELLSPACING=\"0\""
        << " CELLPADDING=\"3\" BGCOLOR=\"#F1948A\" COLOR=\"#E74C3C\">\n";
    dot << "      <TR><TD ALIGN=\"CENTER\"><B>b_apuntadores "
        << blockIdx << "</B></TD></TR>\n";
    for (int j = 0; j < 16; j++) {
        dot << "      <TR><TD ALIGN=\"LEFT\">" << pb.b_pointers[j] << "</TD></TR>\n";
    }
    dot << "    </TABLE>>]\n\n";

    // Conectar con sus apuntados
    for (int j = 0; j < 16; j++) {
        if (pb.b_pointers[j] == -1) continue;
        int child = pb.b_pointers[j];
        if (std::find(visitedBlocks.begin(), visitedBlocks.end(), child)
            != visitedBlocks.end()) continue;
        visitedBlocks.push_back(child);

        dot << "  bloque" << blockIdx << " -> bloque" << child << "\n";

        if (level == 1) {
            // Apunta a bloques de datos directamente
            if (inodeType == '1') {
                // carpeta
                FolderBlock fb;
                file.seekg(sb.s_block_start + child * (int)sizeof(FolderBlock));
                file.read((char*)&fb, sizeof(FolderBlock));

                dot << "  bloque" << child << " [label=<\n";
                dot << "    <TABLE BORDER=\"1\" CELLBORDER=\"0\" CELLSPACING=\"0\""
                    << " CELLPADDING=\"3\" BGCOLOR=\"#F1948A\">\n";
                dot << "      <TR><TD COLSPAN=\"2\" ALIGN=\"CENTER\"><B>b_carpeta "
                    << child << "</B></TD></TR>\n";
                for (int k = 0; k < 4; k++) {
                    std::string nm = cleanStr(fb.b_content[k].b_name, 12);
                    dot << "      <TR><TD>" << (nm.empty()?"-":nm)
                        << "</TD><TD>" << fb.b_content[k].b_inodo << "</TD></TR>\n";
                }
                dot << "    </TABLE>>]\n\n";

                for (int k = 0; k < 4; k++) {
                    if (fb.b_content[k].b_inodo == -1) continue;
                    std::string nm = cleanStr(fb.b_content[k].b_name, 12);
                    if (nm == "." || nm == "..") continue;
                    dot << "  bloque" << child << " -> inodo"
                        << fb.b_content[k].b_inodo << "\n";
                    buildTree(file, sb, fb.b_content[k].b_inodo, nm,
                              dot, visitedInodes, visitedBlocks);
                }
            } else {
                // archivo
                FileBlock ff;
                file.seekg(sb.s_block_start + child * (int)sizeof(FileBlock));
                file.read((char*)&ff, sizeof(FileBlock));
                std::string content;
                for (int k = 0; k < 64; k++) {
                    char c = ff.b_content[k];
                    if (c == '\0') break;
                    if (c == '<') content += "&lt;";
                    else if (c == '>') content += "&gt;";
                    else if (c == '&') content += "&amp;";
                    else if (c == '\n') content += "\\n";
                    else content += c;
                }
                dot << "  bloque" << child << " [label=<\n";
                dot << "    <TABLE BORDER=\"1\" CELLBORDER=\"0\" CELLSPACING=\"0\""
                    << " CELLPADDING=\"3\" BGCOLOR=\"#F9E79F\">\n";
                dot << "      <TR><TD><B>b_archivos " << child << "</B></TD></TR>\n";
                dot << "      <TR><TD><FONT POINT-SIZE=\"9\">"
                    << (content.empty()?"-":content) << "</FONT></TD></TR>\n";
                dot << "    </TABLE>>]\n\n";
            }
        } else {
            // level==2: apunta a otro PointerBlock
            PointerBlock pb2;
            file.seekg(sb.s_block_start + child * (int)sizeof(PointerBlock));
            file.read((char*)&pb2, sizeof(PointerBlock));
            renderPointerBlock(file, sb, child, pb2, inodeType,
                               dot, visitedInodes, visitedBlocks, 1);
        }
    }
}

inline std::string permToUnix(const char* perm, char type) {
    std::string p(perm, 3);
    std::string result = (type == '1') ? "d" : "-";

    auto digitToBits = [](char c) -> int {
        if (c >= '0' && c <= '7') return c - '0';
        return 0;
    };

    for (int i = 0; i < 3; i++) {
        int bits = (i < (int)p.size()) ? digitToBits(p[i]) : 0;
        result += (bits & 4) ? 'r' : '-';
        result += (bits & 2) ? 'w' : '-';
        result += (bits & 1) ? 'x' : '-';
    }
    return result;
}


// ─── 1. REPORTE MBR ──────────────────────────────────────────────────────────
// Estilo del ejemplo: header morado oscuro, primarias en morado,
// particiones lógicas (EBR) en rojo/salmón

inline std::string reportMBR(const std::string& outputPath, const std::string& diskPath) {
    std::ifstream file(diskPath, std::ios::binary);
    if (!file.is_open()) return "Error: no se pudo abrir el disco";

    MBR mbr;
    file.read((char*)&mbr, sizeof(MBR));

    std::ostringstream dot;
    dot << "digraph G {\n";
    dot << "  node [shape=plaintext fontname=\"Arial\"]\n";
    dot << "  tabla [label=<\n";
    dot << "    <TABLE BORDER=\"0\" CELLBORDER=\"1\" CELLSPACING=\"0\" CELLPADDING=\"6\" WIDTH=\"420\">\n";

    // Header principal
    dot << "      <TR>"
        << "<TD COLSPAN=\"2\" BGCOLOR=\"#3E2069\" ALIGN=\"LEFT\">"
        << "<FONT COLOR=\"white\"><B>REPORTE DE MBR</B></FONT>"
        << "</TD></TR>\n";

    // Campos MBR
    auto rowMBR = [&](const std::string& k, const std::string& v, bool alt=false){
        std::string bg = alt ? "#EDE7F6" : "#F3E5F5";
        dot << "      <TR>"
            << "<TD BGCOLOR=\"" << bg << "\" ALIGN=\"RIGHT\"><FONT COLOR=\"#4A148C\">" << k << "</FONT></TD>"
            << "<TD BGCOLOR=\"" << bg << "\" ALIGN=\"LEFT\">" << v << "</TD>"
            << "</TR>\n";
    };

    rowMBR("mbr_tamano",         std::to_string(mbr.mbr_tamano));
    rowMBR("mbr_fecha_creacion", fmtTime(mbr.mbr_fecha_creacion), true);
    rowMBR("mbr_disk_signature", std::to_string(mbr.mbr_dsk_signature));

    // Particiones
    for (int i = 0; i < 4; i++) {
        Partition& p = mbr.mbr_partitions[i];
        bool isExt = (p.part_type == 'E' || p.part_type == 'e');

        // Header partición primaria/extendida
        dot << "      <TR>"
            << "<TD COLSPAN=\"2\" BGCOLOR=\"#3E2069\" ALIGN=\"LEFT\">"
            << "<FONT COLOR=\"white\"><B>Particion</B></FONT>"
            << "</TD></TR>\n";

        auto rowP = [&](const std::string& k, const std::string& v, bool alt=false){
            std::string bg = alt ? "#EDE7F6" : "#F3E5F5";
            dot << "      <TR>"
                << "<TD BGCOLOR=\"" << bg << "\" ALIGN=\"RIGHT\"><FONT COLOR=\"#4A148C\">" << k << "</FONT></TD>"
                << "<TD BGCOLOR=\"" << bg << "\" ALIGN=\"LEFT\">" << v << "</TD>"
                << "</TR>\n";
        };

        rowP("part_status", std::string(1, p.part_status));
        rowP("part_type",   std::string(1, (char)tolower(p.part_type)), true);
        rowP("part_fit",    std::string(1, (char)tolower(p.part_fit)));
        rowP("part_start",  std::to_string(p.part_start), true);
        rowP("part_size",   std::to_string(p.part_s));
        rowP("part_name",   cleanStr(p.part_name, 16), true);

        // Si es extendida, mostrar EBRs (particiones lógicas)
        if (isExt) {
            int ebrPos = p.part_start;
            int extEnd = p.part_start + p.part_s;

            while (ebrPos != -1 && ebrPos >= 0 && ebrPos < extEnd) {
                EBR ebr;
                file.seekg(ebrPos);
                file.read((char*)&ebr, sizeof(EBR));

                // Header partición lógica (rojo/salmón)
                dot << "      <TR>"
                    << "<TD COLSPAN=\"2\" BGCOLOR=\"#C62828\" ALIGN=\"LEFT\">"
                    << "<FONT COLOR=\"white\"><B>Particion Logica</B></FONT>"
                    << "</TD></TR>\n";

                auto rowL = [&](const std::string& k, const std::string& v, bool alt=false){
                    std::string bg = alt ? "#FFCDD2" : "#FFEBEE";
                    dot << "      <TR>"
                        << "<TD BGCOLOR=\"" << bg << "\" ALIGN=\"RIGHT\"><FONT COLOR=\"#B71C1C\">" << k << "</FONT></TD>"
                        << "<TD BGCOLOR=\"" << bg << "\" ALIGN=\"LEFT\">" << v << "</TD>"
                        << "</TR>\n";
                };

                rowL("part_status", std::string(1, ebr.part_status));
                rowL("part_next",   std::to_string(ebr.part_next),  true);
                rowL("part_fit",    std::string(1, (char)tolower(ebr.part_fit)));
                rowL("part_start",  std::to_string(ebr.part_start), true);
                rowL("part_size",   std::to_string(ebr.part_size));
                rowL("part_name",   cleanStr(ebr.part_name, 16),    true);

                if (ebr.part_next <= 0 || ebr.part_next == ebrPos) break;
                ebrPos = ebr.part_next;
            }
        }
    }

    dot << "    </TABLE>>]\n}\n";
    file.close();

    std::string err = renderDot(dot.str(), outputPath);
    if (!err.empty()) return err;
    return "Reporte MBR generado: " + outputPath;
}

// ─── 2. REPORTE DISK ─────────────────────────────────────────────────────────

inline std::string reportDISK(const std::string& outputPath, const std::string& diskPath) {
    std::ifstream file(diskPath, std::ios::binary);
    if (!file.is_open()) return "Error: no se pudo abrir el disco";

    MBR mbr;
    file.read((char*)&mbr, sizeof(MBR));
    int diskSize = mbr.mbr_tamano;

    // Obtener nombre del disco (solo el archivo, sin ruta)
    std::string diskName = diskPath;
    size_t slash = diskPath.find_last_of('/');
    if (slash != std::string::npos) diskName = diskPath.substr(slash + 1);

    struct Seg { std::string label, sublabel; bool isInner; int size; };
    std::vector<Seg> segs;

    // MBR
    segs.push_back({"MBR", "", false, (int)sizeof(MBR)});

    // Ordenar particiones por start
    int order[4] = {0,1,2,3};
    for (int i = 0; i < 3; i++)
        for (int j = i+1; j < 4; j++)
            if (mbr.mbr_partitions[order[i]].part_start >
                mbr.mbr_partitions[order[j]].part_start)
                std::swap(order[i], order[j]);

    int cursor = (int)sizeof(MBR);

    for (int idx = 0; idx < 4; idx++) {
        Partition& p = mbr.mbr_partitions[order[idx]];
        if (p.part_status != '1' || p.part_start < 0) continue;

        // Espacio libre antes de la partición
        if (p.part_start > cursor) {
            int freeSize = p.part_start - cursor;
            int pct = freeSize * 100 / diskSize;
            segs.push_back({"Libre", std::to_string(pct) + "% del disco", false, freeSize});
            cursor = p.part_start;
        }

        bool isExt = (p.part_type == 'E' || p.part_type == 'e');

        if (isExt) {
            int ebrPos = p.part_start;
            int extEnd = p.part_start + p.part_s;
            int cur2   = p.part_start;

            while (ebrPos != -1 && ebrPos >= 0 && ebrPos < extEnd) {
                EBR ebr;
                file.seekg(ebrPos);
                file.read((char*)&ebr, sizeof(EBR));

                // Libre antes del EBR dentro de extendida
                if (ebrPos > cur2) {
                    int fs = ebrPos - cur2;
                    segs.push_back({"Libre", std::to_string(fs*100/diskSize)+"% del Disco",
                                    true, fs});
                }

                // EBR
                segs.push_back({"EBR", "", true, (int)sizeof(EBR)});
                cur2 = ebrPos + (int)sizeof(EBR);

                // Partición lógica
                if (ebr.part_status == '1' && ebr.part_size > 0) {
                    int pct = ebr.part_size * 100 / diskSize;
                    segs.push_back({"Lógica",
                                    std::to_string(pct) + "% del Disco",
                                    true, ebr.part_size});
                    cur2 += ebr.part_size;
                }

                if (ebr.part_next <= 0 || ebr.part_next == ebrPos) break;
                ebrPos = ebr.part_next;
            }

            // Libre al final de extendida
            if (cur2 < extEnd) {
                int fs = extEnd - cur2;
                segs.push_back({"Libre", std::to_string(fs*100/diskSize)+"% del Disco",
                                true, fs});
            }
            cursor = extEnd;

        } else {
            int pct = p.part_s * 100 / diskSize;
            segs.push_back({"Primaria",
                            std::to_string(pct) + "% del disco",
                            false, p.part_s});
            cursor = p.part_start + p.part_s;
        }
    }

    // Libre al final del disco
    if (cursor < diskSize) {
        int fs = diskSize - cursor;
        segs.push_back({"Libre", std::to_string(fs*100/diskSize)+"% del disco",
                        false, fs});
    }

    file.close();

    // Contar segmentos internos (dentro de extendida)
    int innerCount = 0;
    for (auto& s : segs) if (s.isInner) innerCount++;

    std::ostringstream dot;
    dot << "digraph G {\n";
    dot << "  graph [fontname=\"Arial\"]\n";
    dot << "  node  [shape=plaintext fontname=\"Arial\"]\n\n";

    // Título con nombre del disco
    dot << "  title [label=<\n";
    dot << "    <TABLE BORDER=\"0\" CELLBORDER=\"0\">\n";
    dot << "      <TR><TD><FONT POINT-SIZE=\"14\"><B>" << diskName << "</B></FONT></TD></TR>\n";
    dot << "    </TABLE>>]\n\n";

    // Tabla del disco
    dot << "  disco [label=<\n";
    dot << "    <TABLE BORDER=\"1\" CELLBORDER=\"1\" CELLSPACING=\"0\" CELLPADDING=\"12\" COLOR=\"#5B9BD5\">\n";

    // Fila 1: "Extendida" sobre los segmentos internos
    if (innerCount > 0) {
        dot << "      <TR>\n";
        bool extWritten = false;

        // Celdas vacías para segmentos no-inner que van antes
        for (auto& s : segs) {
            if (!s.isInner) {
                if (!extWritten) {
                    dot << "        <TD BORDER=\"0\"></TD>\n";
                } else {
                    dot << "        <TD BORDER=\"0\"></TD>\n";
                }
            } else if (!extWritten) {
                extWritten = true;
                dot << "        <TD COLSPAN=\"" << innerCount
                    << "\" ALIGN=\"CENTER\"><B>Extendida</B></TD>\n";
            }
        }
        dot << "      </TR>\n";
    }

    // Fila 2: contenido real
    dot << "      <TR>\n";
    for (auto& s : segs) {
        if (s.sublabel.empty()) {
            dot << "        <TD ALIGN=\"CENTER\"><B>" << s.label << "</B></TD>\n";
        } else {
            dot << "        <TD ALIGN=\"CENTER\"><B>" << s.label << "</B><BR/>"
                << "<FONT POINT-SIZE=\"10\">" << s.sublabel << "</FONT></TD>\n";
        }
    }
    dot << "      </TR>\n";
    dot << "    </TABLE>>]\n\n";

    // Conectar título con tabla
    dot << "  title -> disco [style=invis]\n";
    dot << "}\n";

    std::string err = renderDot(dot.str(), outputPath);
    if (!err.empty()) return err;
    return "Reporte DISK generado: " + outputPath;
}

// ─── 3. REPORTE SUPERBLOQUE ──────────────────────────────────────────────────

inline std::string reportSB(const std::string& outputPath,
                             const std::string& diskPath, int partStart) {
    std::ifstream file(diskPath, std::ios::binary);
    if (!file.is_open()) return "Error: no se pudo abrir el disco";

    Superblock sb;
    file.seekg(partStart);
    file.read((char*)&sb, sizeof(Superblock));
    file.close();

    // Obtener nombre del disco
    std::string diskName = diskPath;
    size_t slash = diskPath.find_last_of('/');
    if (slash != std::string::npos) diskName = diskPath.substr(slash + 1);

    // Magic number en hex
    std::ostringstream magic;
    magic << "0x" << std::hex << std::uppercase << sb.s_magic;

    std::ostringstream dot;
    dot << "digraph G {\n";
    dot << "  node [shape=plaintext fontname=\"Arial\"]\n\n";

    dot << "  sb [label=<\n";
    dot << "    <TABLE BORDER=\"0\" CELLBORDER=\"1\" CELLSPACING=\"0\""
        << " CELLPADDING=\"6\" WIDTH=\"400\">\n";

    // Header
    dot << "      <TR>"
        << "<TD COLSPAN=\"2\" BGCOLOR=\"#1B5E20\" ALIGN=\"LEFT\">"
        << "<FONT COLOR=\"white\"><B>Reporte de SUPERBLOQUE</B></FONT>"
        << "</TD></TR>\n";

    // Función lambda para filas alternadas
    int rowIdx = 0;
    auto row = [&](const std::string& k, const std::string& v) {
        std::string bg = (rowIdx % 2 == 0) ? "#A5D6A7" : "#FFFFFF";
        rowIdx++;
        dot << "      <TR>"
            << "<TD BGCOLOR=\"" << bg << "\" ALIGN=\"RIGHT\">"
            << "<FONT COLOR=\"#1B5E20\">" << k << "</FONT></TD>"
            << "<TD BGCOLOR=\"" << bg << "\" ALIGN=\"LEFT\">" << v << "</TD>"
            << "</TR>\n";
    };

    row("sb_nombre_hd",              diskName);
    row("sb_inodos_count",           std::to_string(sb.s_inodes_count));
    row("sb_bloques_count",          std::to_string(sb.s_blocks_count));
    row("sb_inodos_free",            std::to_string(sb.s_free_inodes_count));
    row("sb_bloques_free",           std::to_string(sb.s_free_blocks_count));
    row("sb_date_creacion",          fmtTime(sb.s_mtime));
    row("sb_date_ultimo_montaje",    fmtTime(sb.s_umtime));
    row("sb_montajes_count",         std::to_string(sb.s_mnt_count));
    row("sb_magic_num",              magic.str());
    row("sb_size_struct_inodo",      std::to_string(sb.s_inode_size));
    row("sb_size_struct_bloque",     std::to_string(sb.s_block_size));
    row("sb_first_free_bit_inodos",  std::to_string(sb.s_first_ino));
    row("sb_first_free_bit_bloques", std::to_string(sb.s_first_blo));
    row("sb_ap_bitmap_inodos",       std::to_string(sb.s_bm_inode_start));
    row("sb_ap_bitmap_bloques",      std::to_string(sb.s_bm_block_start));
    row("sb_ap_inodos",              std::to_string(sb.s_inode_start));
    row("sb_ap_bloques",             std::to_string(sb.s_block_start));

    dot << "    </TABLE>>]\n\n";

    // Subtítulo debajo
    dot << "  caption [label=\"Reporte de SUPERBLOQUE\""
        << " shape=plaintext fontname=\"Arial\" fontsize=\"10\"]\n";
    dot << "  sb -> caption [style=invis]\n";

    dot << "}\n";

    std::string err = renderDot(dot.str(), outputPath);
    if (!err.empty()) return err;
    return "Reporte SB generado: " + outputPath;
}

// ─── 4. REPORTE INODE ────────────────────────────────────────────────────────

inline std::string reportInode(const std::string& outputPath,
                                const std::string& diskPath, int partStart) {
    std::ifstream file(diskPath, std::ios::binary);
    if (!file.is_open()) return "Error: no se pudo abrir el disco";

    Superblock sb;
    file.seekg(partStart);
    file.read((char*)&sb, sizeof(Superblock));

    std::ostringstream dot;
    dot << "digraph G {\n";
    dot << "  rankdir=LR\n";
    dot << "  node [shape=plaintext fontname=\"Arial\"]\n";
    dot << "  edge [color=\"#5B9BD5\" arrowsize=1.2]\n\n";

    std::vector<int> usedInodes;

    for (int i = 0; i < sb.s_inodes_count; i++) {
        char bit;
        file.seekg(sb.s_bm_inode_start + i);
        file.read(&bit, 1);
        if (bit != '1') continue;

        Inode inode;
        file.seekg(sb.s_inode_start + i * (int)sizeof(Inode));
        file.read((char*)&inode, sizeof(Inode));

        usedInodes.push_back(i);

        std::string perm = cleanStr(inode.i_perm, 3);

        dot << "  inode" << i << " [label=<\n";
        dot << "    <TABLE BORDER=\"1\" CELLBORDER=\"0\" CELLSPACING=\"0\""
            << " CELLPADDING=\"4\" WIDTH=\"220\">\n";

        // Título centrado
        dot << "      <TR><TD COLSPAN=\"2\" ALIGN=\"CENTER\">"
            << "<B>Inodo " << i << "</B></TD></TR>\n";

        // Campos principales
        auto row = [&](const std::string& k, const std::string& v){
            dot << "      <TR>"
                << "<TD ALIGN=\"LEFT\">" << k << "</TD>"
                << "<TD ALIGN=\"LEFT\">" << v << "</TD>"
                << "</TR>\n";
        };

        auto dotRow = [&](){
            dot << "      <TR><TD COLSPAN=\"2\" ALIGN=\"LEFT\">.</TD></TR>\n";
        };

        row("i_uid",   std::to_string(inode.i_uid));
        row("i_size",  std::to_string(inode.i_size));
        row("i_atime", fmtTime(inode.i_atime));
        dotRow();

        // Mostrar bloques asignados con nombre i_block_N (base 1 como en el ejemplo)
        for (int b = 0; b < 15; b++) {
            row("i_block_" + std::to_string(b + 1),
                std::to_string(inode.i_block[b]));
        }

        dotRow();
        row("i_perm", perm.empty() ? "664" : perm);
        dotRow();

        dot << "    </TABLE>>]\n\n";
    }

    // Flechas entre inodos consecutivos
    for (int i = 0; i + 1 < (int)usedInodes.size(); i++) {
        dot << "  inode" << usedInodes[i]
            << " -> inode" << usedInodes[i + 1] << "\n";
    }

    dot << "}\n";
    file.close();

    std::string err = renderDot(dot.str(), outputPath);
    if (!err.empty()) return err;
    return "Reporte INODE generado: " + outputPath;
}

// ─── 5. REPORTE BLOCK ────────────────────────────────────────────────────────

inline std::string reportBlock(const std::string& outputPath,
                                const std::string& diskPath, int partStart) {
    std::fstream file(diskPath, std::ios::in | std::ios::binary);
    if (!file.is_open()) return "Error: no se pudo abrir el disco";

    Superblock sb;
    file.seekg(partStart);
    file.read((char*)&sb, sizeof(Superblock));

    // Determinar tipo de cada bloque leyendo los inodos
    std::vector<char> btype(sb.s_blocks_count, '?');

    for (int i = 0; i < sb.s_inodes_count; i++) {
        char bit;
        file.seekg(sb.s_bm_inode_start + i);
        file.read(&bit, 1);
        if (bit != '1') continue;

        Inode inode;
        file.seekg(sb.s_inode_start + i * (int)sizeof(Inode));
        file.read((char*)&inode, sizeof(Inode));

        char t = (inode.i_type == '1') ? 'D' : 'F';

        for (int b = 0; b < 12; b++)
            if (inode.i_block[b] >= 0 && inode.i_block[b] < sb.s_blocks_count)
                btype[inode.i_block[b]] = t;

        if (inode.i_block[12] >= 0 && inode.i_block[12] < sb.s_blocks_count)
            btype[inode.i_block[12]] = 'P';
        if (inode.i_block[13] >= 0 && inode.i_block[13] < sb.s_blocks_count)
            btype[inode.i_block[13]] = 'P';
    }

    std::ostringstream dot;
    dot << "digraph G {\n";
    dot << "  rankdir=LR\n";
    dot << "  node [shape=plaintext fontname=\"Arial\"]\n";
    dot << "  edge [color=\"#5B9BD5\" arrowsize=1.2]\n\n";

    std::vector<int> usedBlocks;

    for (int i = 0; i < sb.s_blocks_count; i++) {
        char bit;
        file.seekg(sb.s_bm_block_start + i);
        file.read(&bit, 1);
        if (bit != '1') continue;

        char t = btype[i];
        usedBlocks.push_back(i);

        if (t == 'D') {
            // ── Bloque Carpeta ────────────────────────────────────────────────
            FolderBlock fb;
            file.seekg(sb.s_block_start + i * (int)sizeof(FolderBlock));
            file.read((char*)&fb, sizeof(FolderBlock));

            dot << "  block" << i << " [label=<\n";
            dot << "    <TABLE BORDER=\"1\" CELLBORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"4\">\n";

            // Título en negrita, centrado, dos columnas
            dot << "      <TR><TD COLSPAN=\"2\" ALIGN=\"CENTER\">"
                << "<B>Bloque Carpeta " << i << "</B></TD></TR>\n";

            // Encabezados de columna
            dot << "      <TR>"
                << "<TD ALIGN=\"LEFT\"><B>b_name</B></TD>"
                << "<TD ALIGN=\"LEFT\"><B>b_inodo</B></TD>"
                << "</TR>\n";

            // Entradas
            for (int j = 0; j < 4; j++) {
                std::string nm = cleanStr(fb.b_content[j].b_name, 12);
                dot << "      <TR>"
                    << "<TD ALIGN=\"LEFT\">" << (nm.empty() ? "-" : nm) << "</TD>"
                    << "<TD ALIGN=\"LEFT\">" << fb.b_content[j].b_inodo << "</TD>"
                    << "</TR>\n";
            }

            dot << "    </TABLE>>]\n\n";

        } else if (t == 'F') {
            // ── Bloque Archivo ────────────────────────────────────────────────
            FileBlock fb;
            file.seekg(sb.s_block_start + i * (int)sizeof(FileBlock));
            file.read((char*)&fb, sizeof(FileBlock));

            // Escapar contenido para HTML de Graphviz
            std::string content;
            for (int j = 0; j < 64; j++) {
                char c = fb.b_content[j];
                if (c == '\0') break;
                if      (c == '<') content += "&lt;";
                else if (c == '>') content += "&gt;";
                else if (c == '&') content += "&amp;";
                else if (c == '"') content += "&quot;";
                else if (c == '\n') content += "\\n";
                else content += c;
            }

            dot << "  block" << i << " [label=<\n";
            dot << "    <TABLE BORDER=\"1\" CELLBORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"8\">\n";
            dot << "      <TR><TD ALIGN=\"CENTER\"><B>Bloque Archivo " << i << "</B></TD></TR>\n";
            dot << "      <TR><TD ALIGN=\"LEFT\">"
                << (content.empty() ? "-" : content)
                << "</TD></TR>\n";
            dot << "    </TABLE>>]\n\n";

        } else if (t == 'P') {
            // ── Bloque Apuntadores ────────────────────────────────────────────
            PointerBlock pb;
            file.seekg(sb.s_block_start + i * (int)sizeof(PointerBlock));
            file.read((char*)&pb, sizeof(PointerBlock));

            // Construir lista de punteros separados por coma
            // como en el ejemplo: "15, 16, 17, ... -1, -1,"
            // Se muestran todos los 16, agrupados ~6 por línea
            std::string ptrContent;
            for (int j = 0; j < 16; j++) {
                ptrContent += std::to_string(pb.b_pointers[j]);
                ptrContent += ",";
                // Salto de línea cada 6 valores para que no sea muy ancho
                if ((j + 1) % 6 == 0 && j != 15)
                    ptrContent += "<BR/>";
                else if (j != 15)
                    ptrContent += " ";
            }

            dot << "  block" << i << " [label=<\n";
            dot << "    <TABLE BORDER=\"1\" CELLBORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"8\">\n";
            dot << "      <TR><TD ALIGN=\"CENTER\"><B>Bloque Apuntadores " << i << "</B></TD></TR>\n";
            dot << "      <TR><TD ALIGN=\"LEFT\">" << ptrContent << "</TD></TR>\n";
            dot << "    </TABLE>>]\n\n";

        } else {
            // Bloque en uso pero tipo desconocido — mostrar genérico
            dot << "  block" << i << " [label=<\n";
            dot << "    <TABLE BORDER=\"1\" CELLBORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"4\">\n";
            dot << "      <TR><TD><B>Bloque " << i << "</B></TD></TR>\n";
            dot << "    </TABLE>>]\n\n";
        }
    }

    // Flechas entre bloques consecutivos usados
    for (int i = 0; i + 1 < (int)usedBlocks.size(); i++) {
        dot << "  block" << usedBlocks[i]
            << " -> block" << usedBlocks[i + 1] << "\n";
    }

    dot << "}\n";
    file.close();

    std::string err = renderDot(dot.str(), outputPath);
    if (!err.empty()) return err;
    return "Reporte BLOCK generado: " + outputPath;
}

// ─── 6. REPORTE BM_INODE ─────────────────────────────────────────────────────

inline std::string reportBmInode(const std::string& outputPath,
                                  const std::string& diskPath, int partStart) {
    std::ifstream file(diskPath, std::ios::binary);
    if (!file.is_open()) return "Error: no se pudo abrir el disco";

    Superblock sb;
    file.seekg(partStart);
    file.read((char*)&sb, sizeof(Superblock));

    createDirectories(getParentPath(outputPath));
    std::ofstream out(outputPath);
    if (!out.is_open()) { file.close(); return "Error: no se pudo crear el archivo"; }

    file.seekg(sb.s_bm_inode_start);
    for (int i = 0; i < sb.s_inodes_count; i++) {
        char bit;
        file.read(&bit, 1);
        out << bit;
        if ((i + 1) % 20 == 0) {
            out << "\n";   // salto de línea cada 20
        } else {
            out << " ";    // espacio entre bits
        }
    }
    out << "\n";
    out.close();
    file.close();
    return "Reporte BM_INODE generado: " + outputPath;
}

// ─── 7. REPORTE BM_BLOCK ─────────────────────────────────────────────────────

inline std::string reportBmBlock(const std::string& outputPath,
                                  const std::string& diskPath, int partStart) {
    std::ifstream file(diskPath, std::ios::binary);
    if (!file.is_open()) return "Error: no se pudo abrir el disco";

    Superblock sb;
    file.seekg(partStart);
    file.read((char*)&sb, sizeof(Superblock));

    createDirectories(getParentPath(outputPath));
    std::ofstream out(outputPath);
    if (!out.is_open()) { file.close(); return "Error: no se pudo crear el archivo"; }

    file.seekg(sb.s_bm_block_start);
    for (int i = 0; i < sb.s_blocks_count; i++) {
        char bit;
        file.read(&bit, 1);
        out << bit;
        if ((i + 1) % 20 == 0) {
            out << "\n";
        } else {
            out << " ";
        }
    }
    out << "\n";
    out.close();
    file.close();
    return "Reporte BM_BLOCK generado: " + outputPath;
}

// ─── 8. REPORTE TREE ─────────────────────────────────────────────────────────

inline void buildTree(std::fstream& file, Superblock& sb,
                       int inodeIdx, const std::string& label,
                       std::ostringstream& dot, std::vector<int>& visitedInodes,
                       std::vector<int>& visitedBlocks) {

    if (std::find(visitedInodes.begin(), visitedInodes.end(), inodeIdx)
        != visitedInodes.end()) return;
    visitedInodes.push_back(inodeIdx);

    Inode inode;
    file.seekg(sb.s_inode_start + inodeIdx * (int)sizeof(Inode));
    file.read((char*)&inode, sizeof(Inode));

    std::string perm = cleanStr(inode.i_perm, 3);
    bool isDir = (inode.i_type == '1');

    // ── Nodo inodo (AZUL) ─────────────────────────────────────────────────────
    // Label flotante encima
    if (!label.empty()) {
        dot << "  lbl_i" << inodeIdx
            << " [label=\"" << label << "\" shape=plaintext fontname=\"Arial\"]\n";
        dot << "  lbl_i" << inodeIdx << " -> inodo" << inodeIdx
            << " [style=dashed color=\"#888888\" arrowhead=none]\n";
    }

    dot << "  inodo" << inodeIdx << " [label=<\n";
    dot << "    <TABLE BORDER=\"1\" CELLBORDER=\"0\" CELLSPACING=\"0\""
        << " CELLPADDING=\"3\" BGCOLOR=\"#AED6F1\">\n";
    dot << "      <TR><TD COLSPAN=\"2\" ALIGN=\"CENTER\"><B>inodo " << inodeIdx
        << "</B></TD></TR>\n";
    dot << "      <TR><TD ALIGN=\"LEFT\">i_TYPE</TD>"
        << "<TD ALIGN=\"LEFT\">" << (isDir ? "1" : "0") << "</TD></TR>\n";

    // Mostrar ap0..ap14 solo los que no son -1, máximo mostramos ap0,ap1,ap2
    for (int b = 0; b < 15; b++) {
        dot << "      <TR><TD ALIGN=\"LEFT\">ap" << b << "</TD>"
            << "<TD ALIGN=\"LEFT\">" << inode.i_block[b] << "</TD></TR>\n";
    }

    dot << "      <TR><TD ALIGN=\"LEFT\">i_perm</TD>"
        << "<TD ALIGN=\"LEFT\">" << (perm.empty() ? "664" : perm) << "</TD></TR>\n";
    dot << "    </TABLE>>]\n\n";

    // ── Procesar bloques del inodo ────────────────────────────────────────────

    // Bloques directos [0..11]
    for (int b = 0; b < 12; b++) {
        if (inode.i_block[b] == -1) continue;
        int blockIdx = inode.i_block[b];
        if (std::find(visitedBlocks.begin(), visitedBlocks.end(), blockIdx)
            != visitedBlocks.end()) continue;
        visitedBlocks.push_back(blockIdx);

        dot << "  inodo" << inodeIdx << " -> bloque" << blockIdx << "\n";

        if (isDir) {
            // ── Bloque carpeta (ROJO) ─────────────────────────────────────────
            FolderBlock fb;
            file.seekg(sb.s_block_start + blockIdx * (int)sizeof(FolderBlock));
            file.read((char*)&fb, sizeof(FolderBlock));

            dot << "  bloque" << blockIdx << " [label=<\n";
            dot << "    <TABLE BORDER=\"1\" CELLBORDER=\"0\" CELLSPACING=\"0\""
                << " CELLPADDING=\"3\" BGCOLOR=\"#F1948A\">\n";
            dot << "      <TR><TD COLSPAN=\"2\" ALIGN=\"CENTER\"><B>b_carpeta "
                << blockIdx << "</B></TD></TR>\n";

            for (int j = 0; j < 4; j++) {
                std::string nm = cleanStr(fb.b_content[j].b_name, 12);
                dot << "      <TR>"
                    << "<TD ALIGN=\"LEFT\">" << (nm.empty() ? "-" : nm) << "</TD>"
                    << "<TD ALIGN=\"LEFT\">" << fb.b_content[j].b_inodo << "</TD>"
                    << "</TR>\n";
            }
            dot << "    </TABLE>>]\n\n";

            // Recursión para cada entrada válida
            for (int j = 0; j < 4; j++) {
                if (fb.b_content[j].b_inodo == -1) continue;
                std::string nm = cleanStr(fb.b_content[j].b_name, 12);
                if (nm == "." || nm == "..") continue;

                int childInode = fb.b_content[j].b_inodo;
                dot << "  bloque" << blockIdx << " -> inodo" << childInode << "\n";
                buildTree(file, sb, childInode, nm, dot, visitedInodes, visitedBlocks);
            }

        } else {
            // ── Bloque archivo (AMARILLO) ─────────────────────────────────────
            FileBlock ff;
            file.seekg(sb.s_block_start + blockIdx * (int)sizeof(FileBlock));
            file.read((char*)&ff, sizeof(FileBlock));

            std::string content;
            for (int j = 0; j < 64; j++) {
                char c = ff.b_content[j];
                if (c == '\0') break;
                if      (c == '<') content += "&lt;";
                else if (c == '>') content += "&gt;";
                else if (c == '&') content += "&amp;";
                else if (c == '\n') content += "\\n";
                else content += c;
            }

            dot << "  bloque" << blockIdx << " [label=<\n";
            dot << "    <TABLE BORDER=\"1\" CELLBORDER=\"0\" CELLSPACING=\"0\""
                << " CELLPADDING=\"3\" BGCOLOR=\"#F9E79F\">\n";
            dot << "      <TR><TD ALIGN=\"CENTER\"><B>b_archivos "
                << blockIdx << "</B></TD></TR>\n";
            dot << "      <TR><TD ALIGN=\"LEFT\"><FONT POINT-SIZE=\"9\">"
                << (content.empty() ? "-" : content) << "</FONT></TD></TR>\n";
            dot << "    </TABLE>>]\n\n";
        }
    }

    // ── Bloque indirecto simple [12] ──────────────────────────────────────────
    if (inode.i_block[12] != -1) {
        int ptrIdx = inode.i_block[12];
        if (std::find(visitedBlocks.begin(), visitedBlocks.end(), ptrIdx)
            == visitedBlocks.end()) {
            visitedBlocks.push_back(ptrIdx);

            PointerBlock pb;
            file.seekg(sb.s_block_start + ptrIdx * (int)sizeof(PointerBlock));
            file.read((char*)&pb, sizeof(PointerBlock));

            dot << "  inodo" << inodeIdx << " -> bloque" << ptrIdx << "\n";
            renderPointerBlock(file, sb, ptrIdx, pb, inode.i_type,
                               dot, visitedInodes, visitedBlocks, 1);
        }
    }

    // ── Bloque doblemente indirecto [13] ──────────────────────────────────────
    if (inode.i_block[13] != -1) {
        int ptrIdx = inode.i_block[13];
        if (std::find(visitedBlocks.begin(), visitedBlocks.end(), ptrIdx)
            == visitedBlocks.end()) {
            visitedBlocks.push_back(ptrIdx);

            PointerBlock pb;
            file.seekg(sb.s_block_start + ptrIdx * (int)sizeof(PointerBlock));
            file.read((char*)&pb, sizeof(PointerBlock));

            dot << "  inodo" << inodeIdx << " -> bloque" << ptrIdx << "\n";
            renderPointerBlock(file, sb, ptrIdx, pb, inode.i_type,
                               dot, visitedInodes, visitedBlocks, 2);
        }
    }
}

inline std::string reportTree(const std::string& outputPath,
                               const std::string& diskPath, int partStart) {
    std::fstream file(diskPath, std::ios::in | std::ios::binary);
    if (!file.is_open()) return "Error: no se pudo abrir el disco";

    Superblock sb;
    file.seekg(partStart);
    file.read((char*)&sb, sizeof(Superblock));

    std::ostringstream dot;
    dot << "digraph G {\n";
    dot << "  rankdir=LR\n";
    dot << "  node [shape=plaintext fontname=\"Arial\" fontsize=\"10\"]\n";
    dot << "  edge [color=\"#2C3E50\" arrowsize=0.8]\n\n";

    std::vector<int> visitedInodes;
    std::vector<int> visitedBlocks;

    // Empezar desde la raíz (inodo 0), label "inodo de carpeta raiz"
    buildTree(file, sb, 0, "inodo de carpeta\\nraiz",
              dot, visitedInodes, visitedBlocks);

    dot << "}\n";
    file.close();

    std::string err = renderDot(dot.str(), outputPath);
    if (!err.empty()) return err;
    return "Reporte TREE generado: " + outputPath;
}

// ─── 9. REPORTE FILE ─────────────────────────────────────────────────────────

inline std::string reportFile(const std::string& outputPath,
                               const std::string& diskPath, int partStart,
                               const std::string& filePath) {
    if (filePath.empty())
        return "Error: rep file requiere -path_file_ls";

    std::fstream file(diskPath, std::ios::in | std::ios::binary);
    if (!file.is_open()) return "Error: no se pudo abrir el disco";

    Superblock sb;
    file.seekg(partStart);
    file.read((char*)&sb, sizeof(Superblock));

    // ── Navegar la ruta para encontrar el inodo ───────────────────────────────
    std::vector<std::string> parts;
    std::stringstream ss(filePath);
    std::string item;
    while (std::getline(ss, item, '/'))
        if (!item.empty()) parts.push_back(item);

    if (parts.empty()) {
        file.close();
        return "Error: ruta invalida";
    }

    int cur = 0;  // empezar en raiz (inodo 0)
    for (auto& p : parts) {
        Inode inode;
        file.seekg(sb.s_inode_start + cur * (int)sizeof(Inode));
        file.read((char*)&inode, sizeof(Inode));

        if (inode.i_type != '1') {
            file.close();
            return "Error: '" + p + "' no es un directorio";
        }

        bool found = false;
        for (int b = 0; b < 12 && !found; b++) {
            if (inode.i_block[b] == -1) continue;
            FolderBlock fb;
            file.seekg(sb.s_block_start + inode.i_block[b] * (int)sizeof(FolderBlock));
            file.read((char*)&fb, sizeof(FolderBlock));
            for (int j = 0; j < 4 && !found; j++) {
                if (fb.b_content[j].b_inodo == -1) continue;
                if (cleanStr(fb.b_content[j].b_name, 12) == p) {
                    cur = fb.b_content[j].b_inodo;
                    found = true;
                }
            }
        }
        if (!found) {
            file.close();
            return "Error: '" + p + "' no existe en la ruta";
        }
    }

    // ── Verificar que sea archivo ─────────────────────────────────────────────
    Inode target;
    file.seekg(sb.s_inode_start + cur * (int)sizeof(Inode));
    file.read((char*)&target, sizeof(Inode));

    if (target.i_type != '0') {
        file.close();
        return "Error: la ruta especificada es un directorio, no un archivo";
    }

    // ── Leer contenido completo ───────────────────────────────────────────────
    std::string content = readInodeContent(file, sb, target);
    file.close();

    // ── Escribir en el archivo de salida ─────────────────────────────────────
    createDirectories(getParentPath(outputPath));
    std::ofstream out(outputPath);
    if (!out.is_open())
        return "Error: no se pudo crear el archivo de salida";

    // Nombre del archivo (último segmento de la ruta)
    out << "Archivo: " << parts.back() << "\n";
    out << "Ruta: "    << filePath      << "\n";
    out << "Tamano: "  << target.i_size << " bytes\n";
    out << std::string(40, '-') << "\n";
    out << content;

    out.close();
    return "Reporte FILE generado: " + outputPath;
}

// ─── 10. REPORTE LS ──────────────────────────────────────────────────────────

inline std::string reportLS(const std::string& outputPath,
                             const std::string& diskPath, int partStart,
                             const std::string& dirPath) {
    if (dirPath.empty())
        return "Error: rep ls requiere -path_file_ls";

    std::fstream file(diskPath, std::ios::in | std::ios::binary);
    if (!file.is_open()) return "Error: no se pudo abrir el disco";

    Superblock sb;
    file.seekg(partStart);
    file.read((char*)&sb, sizeof(Superblock));

    // ── Navegar a la ruta indicada ────────────────────────────────────────────
    std::vector<std::string> parts;
    std::stringstream ss(dirPath);
    std::string item;
    while (std::getline(ss, item, '/'))
        if (!item.empty()) parts.push_back(item);

    int cur = 0;  // raíz
    for (auto& p : parts) {
        Inode inode;
        file.seekg(sb.s_inode_start + cur * (int)sizeof(Inode));
        file.read((char*)&inode, sizeof(Inode));

        bool found = false;
        for (int b = 0; b < 12 && !found; b++) {
            if (inode.i_block[b] == -1) continue;
            FolderBlock fb;
            file.seekg(sb.s_block_start + inode.i_block[b] * (int)sizeof(FolderBlock));
            file.read((char*)&fb, sizeof(FolderBlock));
            for (int j = 0; j < 4 && !found; j++) {
                if (fb.b_content[j].b_inodo == -1) continue;
                if (cleanStr(fb.b_content[j].b_name, 12) == p) {
                    cur = fb.b_content[j].b_inodo;
                    found = true;
                }
            }
        }
        if (!found) {
            file.close();
            return "Error: directorio no encontrado '" + p + "'";
        }
    }

    Inode dirInode;
    file.seekg(sb.s_inode_start + cur * (int)sizeof(Inode));
    file.read((char*)&dirInode, sizeof(Inode));

    if (dirInode.i_type != '1') {
        file.close();
        return "Error: la ruta no es un directorio";
    }

    // ── Generar DOT ───────────────────────────────────────────────────────────
    std::ostringstream dot;
    dot << "digraph G {\n";
    dot << "  node [shape=plaintext fontname=\"Arial\"]\n\n";
    dot << "  ls [label=<\n";
    dot << "    <TABLE BORDER=\"1\" CELLBORDER=\"1\" CELLSPACING=\"0\""
        << " CELLPADDING=\"8\" WIDTH=\"700\">\n";

    // Encabezados — fondo blanco, texto negro bold
    dot << "      <TR>\n"
        << "        <TD BGCOLOR=\"#FFFFFF\"><B>Permisos</B></TD>\n"
        << "        <TD BGCOLOR=\"#FFFFFF\"><B>Owner</B></TD>\n"
        << "        <TD BGCOLOR=\"#FFFFFF\"><B>Grupo</B></TD>\n"
        << "        <TD BGCOLOR=\"#FFFFFF\"><B>Size (en Bytes)</B></TD>\n"
        << "        <TD BGCOLOR=\"#FFFFFF\"><B>Fecha</B></TD>\n"
        << "        <TD BGCOLOR=\"#FFFFFF\"><B>Hora</B></TD>\n"
        << "        <TD BGCOLOR=\"#FFFFFF\"><B>Tipo</B></TD>\n"
        << "        <TD BGCOLOR=\"#FFFFFF\"><B>Name</B></TD>\n"
        << "      </TR>\n";

    // Fila vacía separadora (como en el ejemplo)
    dot << "      <TR>"
        << "<TD></TD><TD></TD><TD></TD><TD></TD>"
        << "<TD></TD><TD></TD><TD></TD><TD></TD>"
        << "</TR>\n";

    // Recolectar entradas del directorio
    for (int b = 0; b < 12; b++) {
        if (dirInode.i_block[b] == -1) continue;

        FolderBlock fb;
        file.seekg(sb.s_block_start + dirInode.i_block[b] * (int)sizeof(FolderBlock));
        file.read((char*)&fb, sizeof(FolderBlock));

        for (int j = 0; j < 4; j++) {
            if (fb.b_content[j].b_inodo == -1) continue;
            std::string nm = cleanStr(fb.b_content[j].b_name, 12);
            if (nm == "." || nm == "..") continue;

            Inode ch;
            file.seekg(sb.s_inode_start + fb.b_content[j].b_inodo * (int)sizeof(Inode));
            file.read((char*)&ch, sizeof(Inode));

            // Permisos en formato Unix
            std::string perms = permToUnix(ch.i_perm, ch.i_type);

            // Fecha y hora separadas
            char fecha[32], hora[16];
            std::tm* ti = localtime(&ch.i_mtime);
            strftime(fecha, sizeof(fecha), "%d/%m/%Y", ti);
            strftime(hora,  sizeof(hora),  "%H:%M",    ti);

            std::string tipo = (ch.i_type == '1') ? "Carpeta" : "Archivo";

            // Owner y Grupo como UID/GID numérico
            // (se pueden mapear a nombres si tienes la tabla de users.txt)
            std::string owner = std::to_string(ch.i_uid);
            std::string grupo = std::to_string(ch.i_gid);

            dot << "      <TR>\n"
                << "        <TD ALIGN=\"LEFT\">" << perms << "</TD>\n"
                << "        <TD ALIGN=\"CENTER\">" << owner << "</TD>\n"
                << "        <TD ALIGN=\"CENTER\">" << grupo << "</TD>\n"
                << "        <TD ALIGN=\"RIGHT\">" << ch.i_size << "</TD>\n"
                << "        <TD ALIGN=\"CENTER\">" << fecha << "</TD>\n"
                << "        <TD ALIGN=\"CENTER\">" << hora << "</TD>\n"
                << "        <TD ALIGN=\"CENTER\">" << tipo << "</TD>\n"
                << "        <TD ALIGN=\"LEFT\">" << nm << "</TD>\n"
                << "      </TR>\n";

            // Fila vacía separadora entre entradas (como en el ejemplo)
            dot << "      <TR>"
                << "<TD></TD><TD></TD><TD></TD><TD></TD>"
                << "<TD></TD><TD></TD><TD></TD><TD></TD>"
                << "</TR>\n";
        }
    }

    dot << "    </TABLE>>]\n}\n";
    file.close();

    std::string err = renderDot(dot.str(), outputPath);
    if (!err.empty()) return err;
    return "Reporte LS generado: " + outputPath;
}

// ─── Dispatcher ───────────────────────────────────────────────────────────────

inline std::string execute(const std::string& name, const std::string& path,
                            const std::string& id, const std::string& pathFileLs) {
    if (name.empty()) return "Error: falta -name";
    if (path.empty()) return "Error: falta -path";
    if (id.empty())   return "Error: falta -id";

    CommandMount::MountedPartition partition;
    if (!CommandMount::getMountedPartition(id, partition))
        return "Error: la particion '" + id + "' no esta montada";

    std::string type = toLowerCase(name);

    if (type == "mbr")      return reportMBR     (path, partition.path);
    if (type == "disk")     return reportDISK    (path, partition.path);
    if (type == "sb")       return reportSB      (path, partition.path, partition.start);
    if (type == "inode")    return reportInode   (path, partition.path, partition.start);
    if (type == "block")    return reportBlock   (path, partition.path, partition.start);
    if (type == "bm_inode") return reportBmInode (path, partition.path, partition.start);
    if (type == "bm_block") return reportBmBlock (path, partition.path, partition.start);
    if (type == "tree")     return reportTree    (path, partition.path, partition.start);
    if (type == "file")     return reportFile    (path, partition.path, partition.start, pathFileLs);
    if (type == "ls")       return reportLS      (path, partition.path, partition.start, pathFileLs);

    return "Error: reporte '" + name + "' no reconocido. Valores validos: mbr, disk, sb, inode, block, bm_inode, bm_block, tree, file, ls";
}

// ─── Parser ───────────────────────────────────────────────────────────────────

inline std::string executeFromLine(const std::string& commandLine) {
    std::istringstream iss(commandLine);
    std::string token, name, path, id, pathFileLs;

    iss >> token;  // saltar "rep"

    while (iss >> token) {
        std::string lower = toLowerCase(token);
        if      (lower.find("-name=")         == 0) name       = token.substr(6);
        else if (lower.find("-path=")         == 0) path       = token.substr(6);
        else if (lower.find("-id=")           == 0) id         = token.substr(4);
        else if (lower.find("-path_file_ls=") == 0) pathFileLs = token.substr(14);
    }

    return execute(name, path, id, pathFileLs);
}

}  // namespace CommandRep

#endif