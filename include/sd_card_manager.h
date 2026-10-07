#pragma once

#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <vector>
#include "config_common.h"

/**
 * SDCardManager - Driver e Utilitário de Sistema de Ficheiros para MicroSD
 * 
 * Especialmente desenhado para a placa Waveshare/Lafvin ESP32-C6 com LCD 1.47" ST7789.
 * O barramento SPI (SCLK=7, MOSI=6, MISO=5) é partilhado entre o ecrã LCD (CS=14)
 * e o slot de cartão MicroSD TF onboard (CS=4).
 */
class SDCardManager {
public:
    SDCardManager() 
        : _initialized(false), _csPin(PIN_SD_CS), _cardType(CARD_NONE),
          _cardSizeMB(0), _totalBytes(0), _usedBytes(0), _freeBytes(0) {}

    /**
     * Inicializa a comunicação com o cartão MicroSD.
     * @param csPin Pino de Chip Select (padrão: PIN_SD_CS = 4)
     * @param spiBus Instância SPI partilhada (padrão: SPI)
     * @param frequency Frequência do barramento SPI (padrão: 20MHz com fallback para 4MHz)
     * @return true se montado com sucesso e cartão válido detectado
     */
    bool begin(uint8_t csPin = PIN_SD_CS, SPIClass &spiBus = SPI, uint32_t frequency = 20000000) {
        _csPin = csPin;

        // 1. Assegurar estados de CS para prevenir colisões no barramento SPI partilhado
        pinMode(PIN_LCD_CS, OUTPUT);
        digitalWrite(PIN_LCD_CS, HIGH); // Deselecionar LCD

        pinMode(_csPin, OUTPUT);
        digitalWrite(_csPin, HIGH);     // Deselecionar SD Card

        // 2. Garantir que o barramento SPI partilhado tem os pinos corretos configurados
        spiBus.begin(PIN_SD_SCLK, PIN_SD_MISO, PIN_SD_MOSI, -1);

        // 3. Tentar montar o cartão SD na frequência pedida (suporta até 16 ficheiros abertos)
        if (!SD.begin(_csPin, spiBus, frequency, "/sd", 16)) {
            // Tentativa de fallback para 4MHz (compatibilidade com cartões mais antigos / cabos)
            if (frequency > 4000000) {
                if (!SD.begin(_csPin, spiBus, 4000000, "/sd", 16)) {
                    _initialized = false;
                    _cardType = CARD_NONE;
                    // Deixa CS em HIGH
                    digitalWrite(_csPin, HIGH);
                    return false;
                }
            } else {
                _initialized = false;
                _cardType = CARD_NONE;
                digitalWrite(_csPin, HIGH);
                return false;
            }
        }

        // 4. Identificar tipo de cartão
        _cardType = SD.cardType();
        if (_cardType == CARD_NONE) {
            _initialized = false;
            SD.end();
            digitalWrite(_csPin, HIGH);
            return false;
        }

        _initialized = true;
        digitalWrite(_csPin, HIGH); // Manter CS em repouso
        refreshStorageInfo();
        return true;
    }

    /**
     * Garante que os pinos de CS e SPI estão devidamente configurados como GPIO Output
     * antes de qualquer operação no cartão MicroSD.
     */
    inline void prepareBus() {
        pinMode(PIN_LCD_CS, OUTPUT);
        digitalWrite(PIN_LCD_CS, HIGH); // Garante que o LCD não escuta o barramento
        pinMode(_csPin, OUTPUT);
    }

    /**
     * Atualiza as informações de espaço em disco em cache
     */
    void refreshStorageInfo() {
        if (!_initialized) return;
        prepareBus();
        _cardSizeMB = SD.cardSize() / (1024ULL * 1024ULL);
        _totalBytes = SD.totalBytes();
        _usedBytes = SD.usedBytes();
        _freeBytes = (_totalBytes > _usedBytes) ? (_totalBytes - _usedBytes) : 0;
        digitalWrite(_csPin, HIGH);
    }

    /**
     * Desmonta e liberta o cartão MicroSD
     */
    void end() {
        if (_initialized) {
            SD.end();
            _initialized = false;
            _cardType = CARD_NONE;
            _cardSizeMB = 0;
            _totalBytes = 0;
            _usedBytes = 0;
            _freeBytes = 0;
            digitalWrite(_csPin, HIGH);
        }
    }

    /**
     * Verifica se o cartão SD foi inicializado e está acessível
     */
    bool isReady() const {
        return _initialized && (_cardType != CARD_NONE);
    }

    /**
     * Retorna o tipo de cartão SD detectado
     */
    uint8_t getCardType() const {
        return _cardType;
    }

    /**
     * Retorna a descrição amigável do tipo de cartão
     */
    const char* getCardTypeStr() const {
        switch (_cardType) {
            case CARD_MMC:     return "MMC";
            case CARD_SD:      return "SDSC";
            case CARD_SDHC:    return "SDHC / SDXC";
            case CARD_UNKNOWN: return "DESCONHECIDO";
            case CARD_NONE:
            default:           return "NENHUM";
        }
    }

    /**
     * Tamanho total do cartão em Megabytes (MB)
     */
    uint64_t getCardSizeMB() const {
        return _initialized ? _cardSizeMB : 0;
    }

    /**
     * Total de bytes da partição FAT formatada
     */
    uint64_t getTotalBytes() const {
        return _initialized ? _totalBytes : 0;
    }

    /**
     * Bytes ocupados na partição
     */
    uint64_t getUsedBytes() const {
        return _initialized ? _usedBytes : 0;
    }

    /**
     * Bytes livres na partição
     */
    uint64_t getFreeBytes() const {
        return _initialized ? _freeBytes : 0;
    }

    /**
     * Verifica se um ficheiro existe
     */
    bool fileExists(const char* path) {
        if (!_initialized || !path) return false;
        prepareBus();
        bool exists = SD.exists(path);
        digitalWrite(_csPin, HIGH);
        return exists;
    }

    /**
     * Obtém o tamanho de um ficheiro em bytes
     */
    size_t getFileSize(const char* path) {
        if (!_initialized || !path) return 0;
        prepareBus();
        File file = SD.open(path, FILE_READ);
        if (!file || file.isDirectory()) {
            if (file) file.close();
            digitalWrite(_csPin, HIGH);
            return 0;
        }
        size_t size = file.size();
        file.close();
        digitalWrite(_csPin, HIGH);
        return size;
    }

    /**
     * Lê o conteúdo completo de um ficheiro de texto em uma String
     * @param path Caminho do ficheiro (ex: "/config.txt")
     * @param maxBytes Limite de bytes para ler (evita estouro de RAM)
     */
    String readFile(const char* path, size_t maxBytes = 4096) {
        if (!_initialized || !path) return String();
        prepareBus();

        File file = SD.open(path, FILE_READ);
        if (!file || file.isDirectory()) {
            if (file) file.close();
            digitalWrite(_csPin, HIGH);
            return String();
        }

        size_t toRead = file.size();
        if (toRead > maxBytes) toRead = maxBytes;

        String content;
        content.reserve(toRead + 1);

        while (file.available() && content.length() < toRead) {
            content += (char)file.read();
        }

        file.close();
        digitalWrite(_csPin, HIGH);
        return content;
    }

    /**
     * Lê bytes binários de um ficheiro
     */
    bool readFileBytes(const char* path, uint8_t* buffer, size_t maxLen, size_t &bytesRead) {
        bytesRead = 0;
        if (!_initialized || !path || !buffer || maxLen == 0) return false;
        prepareBus();

        File file = SD.open(path, FILE_READ);
        if (!file || file.isDirectory()) {
            if (file) file.close();
            digitalWrite(_csPin, HIGH);
            return false;
        }

        bytesRead = file.read(buffer, maxLen);
        file.close();
        digitalWrite(_csPin, HIGH);
        return true;
    }

    /**
     * Escreve (sobrescreve) dados em um ficheiro
     */
    bool writeFile(const char* path, const char* message) {
        if (!_initialized || !path || !message) return false;
        prepareBus();

        File file = SD.open(path, FILE_WRITE);
        if (!file) {
            digitalWrite(_csPin, HIGH);
            return false;
        }

        size_t written = file.print(message);
        file.close();
        digitalWrite(_csPin, HIGH);
        return (written > 0);
    }

    /**
     * Adiciona dados ao fim de um ficheiro (útil para logs e telemetria)
     */
    bool appendFile(const char* path, const char* message) {
        if (!_initialized || !path || !message) return false;
        prepareBus();

        File file = SD.open(path, FILE_APPEND);
        if (!file) {
            digitalWrite(_csPin, HIGH);
            return false;
        }

        size_t written = file.print(message);
        file.close();
        digitalWrite(_csPin, HIGH);
        return (written > 0);
    }

    /**
     * Apaga um ficheiro
     */
    bool deleteFile(const char* path) {
        if (!_initialized || !path) return false;
        prepareBus();
        bool ok = SD.remove(path);
        digitalWrite(_csPin, HIGH);
        if (ok) refreshStorageInfo();
        return ok;
    }

    /**
     * Cria um novo directório
     */
    bool createDir(const char* path) {
        if (!_initialized || !path || strlen(path) == 0) return false;
        prepareBus();
        bool ok = SD.mkdir(path);
        digitalWrite(_csPin, HIGH);
        return ok;
    }

    /**
     * Apaga um directório (recursivamente com todo o conteúdo ou apenas se vazio).
     * Não permite apagar a raiz ("/").
     */
    bool deleteDir(const char* path, bool recursive = true) {
        if (!_initialized || !path || strlen(path) == 0) return false;

        String p = String(path);
        if (!p.startsWith("/")) p = "/" + p;
        while (p.length() > 1 && p.endsWith("/")) {
            p = p.substring(0, p.length() - 1);
        }
        if (p == "/" || p.length() == 0) return false;

        prepareBus();
        bool ok = true;
        if (recursive) {
            ok = removeDirRecursive(p.c_str());
        } else {
            ok = SD.rmdir(p.c_str());
        }
        digitalWrite(_csPin, HIGH);
        if (ok) refreshStorageInfo();
        return ok;
    }

    /**
     * Lista ficheiros e pastas no monitor Serial ou outro Stream
     */
    void printDirectory(const char* dirPath = "/", uint8_t maxDepth = 2, Stream &out = Serial) {
        if (!_initialized) {
            out.println("[SD] Erro: Cartao SD nao montado!");
            return;
        }
        prepareBus();

        File root = SD.open(dirPath);
        if (!root || !root.isDirectory()) {
            out.printf("[SD] Nao foi possivel abrir o diretorio: %s\n", dirPath);
            if (root) root.close();
            digitalWrite(_csPin, HIGH);
            return;
        }

        out.printf("\n--- Conteudo de '%s' (Cartao: %s | %llu MB) ---\n", 
                   dirPath, getCardTypeStr(), getCardSizeMB());
        printDirInternal(root, 0, maxDepth, out);
        out.println("-------------------------------------------------");
        root.close();
        digitalWrite(_csPin, HIGH);
    }

    /**
     * Retorna uma lista com os caminhos dos ficheiros presentes na directoria raiz
     */
    std::vector<String> listFiles(const char* dirPath = "/") {
        std::vector<String> results;
        if (!_initialized) return results;
        prepareBus();

        File root = SD.open(dirPath);
        if (!root || !root.isDirectory()) {
            if (root) root.close();
            digitalWrite(_csPin, HIGH);
            return results;
        }

        File file = root.openNextFile();
        while (file) {
            if (!file.isDirectory()) {
                results.push_back(String(file.name()));
            }
            file = root.openNextFile();
        }
        root.close();
        digitalWrite(_csPin, HIGH);
        return results;
    }

    struct SDFileEntry {
        String name;
        String path;
        size_t size;
        bool isDirectory;
    };

    /**
     * Retorna uma lista detalhada de ficheiros e pastas com tamanhos
     */
    std::vector<SDFileEntry> listEntries(const char* dirPath = "/") {
        std::vector<SDFileEntry> results;
        if (!_initialized) return results;

        prepareBus();

        String targetDir = (dirPath && strlen(dirPath) > 0) ? String(dirPath) : String("/");
        if (!targetDir.startsWith("/")) targetDir = "/" + targetDir;

        File root = SD.open(targetDir.c_str());
        if (!root || !root.isDirectory()) {
            if (root) root.close();
            digitalWrite(_csPin, HIGH);
            return results;
        }

        root.rewindDirectory();

        // Itera sobre ficheiros e subpastas no diretório
        File file = root.openNextFile();
        while (file) {
            SDFileEntry entry;
            const char* fn = file.name();
            entry.name = String(fn ? fn : "");
            // Se o nome vier com caminho absoluto, extrair apenas o basename
            int lastSlash = entry.name.lastIndexOf('/');
            if (lastSlash >= 0) {
                entry.name = entry.name.substring(lastSlash + 1);
            }

            entry.isDirectory = file.isDirectory();
            entry.size = entry.isDirectory ? 0 : file.size();

            const char* fp = file.path();
            if (fp && strlen(fp) > 0) {
                entry.path = String(fp);
            } else {
                entry.path = targetDir.endsWith("/") ? (targetDir + entry.name) : (targetDir + "/" + entry.name);
            }

            file.close(); // Fecha imediatamente o descritor para libertar recursos FatFS
            results.push_back(entry);
            file = root.openNextFile();
        }

        root.close();
        digitalWrite(_csPin, HIGH);
        return results;
    }

    /**
     * Abre diretamente um ficheiro no SD Card para streaming/download
     */
    File openFile(const char* path, const char* mode = FILE_READ) {
        if (!_initialized || !path) return File();
        prepareBus();
        return SD.open(path, mode);
    }

    /**
     * Imprime informação detalhada do cartão SD no Serial
     */
    void printCardInfo(Stream &out = Serial) {
        if (!_initialized) {
            out.println("[SD] Nenhum cartao MicroSD pronto ou detetado.");
            return;
        }
        out.println("==========================================");
        out.println("          MICRO SD CARD DETETADO          ");
        out.println("==========================================");
        out.printf(" Tipo de Cartao : %s\n", getCardTypeStr());
        out.printf(" Capacidade     : %llu MB (%.2f GB)\n", getCardSizeMB(), getCardSizeMB() / 1024.0f);
        out.printf(" Espaco Total   : %llu MB\n", getTotalBytes() / (1024ULL * 1024ULL));
        out.printf(" Espaco Usado   : %llu MB\n", getUsedBytes() / (1024ULL * 1024ULL));
        out.printf(" Espaco Livre   : %llu MB\n", getFreeBytes() / (1024ULL * 1024ULL));
        out.printf(" Pino CS        : GPIO %d\n", _csPin);
        out.println("==========================================");
    }

private:
    bool _initialized;
    uint8_t _csPin;
    uint8_t _cardType;
    uint64_t _cardSizeMB;
    uint64_t _totalBytes;
    uint64_t _usedBytes;
    uint64_t _freeBytes;

    bool removeDirRecursive(const char* dirPath) {
        File dir = SD.open(dirPath);
        if (!dir) return false;
        if (!dir.isDirectory()) {
            dir.close();
            return SD.remove(dirPath);
        }

        struct EntryItem {
            String path;
            bool isDir;
        };
        std::vector<EntryItem> entries;

        String baseDir = String(dirPath);
        if (!baseDir.startsWith("/")) baseDir = "/" + baseDir;
        while (baseDir.length() > 1 && baseDir.endsWith("/")) {
            baseDir = baseDir.substring(0, baseDir.length() - 1);
        }

        File child = dir.openNextFile();
        while (child) {
            EntryItem item;
            item.isDir = child.isDirectory();
            const char* ep = child.path();
            if (ep && strlen(ep) > 0) {
                item.path = String(ep);
            } else {
                const char* fn = child.name();
                String fname = String(fn ? fn : "");
                int lastSlash = fname.lastIndexOf('/');
                if (lastSlash >= 0) fname = fname.substring(lastSlash + 1);
                item.path = baseDir + "/" + fname;
            }
            entries.push_back(item);
            child.close();
            child = dir.openNextFile();
        }
        dir.close();

        bool allOk = true;
        for (const auto& item : entries) {
            if (item.isDir) {
                if (!removeDirRecursive(item.path.c_str())) {
                    allOk = false;
                }
            } else {
                if (!SD.remove(item.path.c_str())) {
                    allOk = false;
                }
            }
        }

        if (!SD.rmdir(dirPath)) {
            allOk = false;
        }

        return allOk;
    }

    void printDirInternal(File &dir, uint8_t currentDepth, uint8_t maxDepth, Stream &out) {
        if (currentDepth > maxDepth) return;

        File file = dir.openNextFile();
        while (file) {
            for (uint8_t i = 0; i < currentDepth; i++) {
                out.print("  ");
            }
            if (file.isDirectory()) {
                out.printf("[DIR]  %s\n", file.name());
                if (currentDepth < maxDepth) {
                    printDirInternal(file, currentDepth + 1, maxDepth, out);
                }
            } else {
                out.printf("[FILE] %-20s %u bytes\n", file.name(), (unsigned int)file.size());
            }
            file = dir.openNextFile();
        }
    }
};
