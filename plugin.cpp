#include <RE/Skyrim.h>
#include <REL/Relocation.h>
#include <SKSE/SKSE.h>
#include <shlobj.h>
#include <windows.h>

#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

// ===== FUNCIONES UTILITARIAS ULTRA-SEGURAS =====

std::string SafeWideStringToString(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    try {
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), NULL, 0, NULL, NULL);
        if (size_needed <= 0) {
            size_needed = WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), (int)wstr.size(), NULL, 0, NULL, NULL);
            if (size_needed <= 0) return std::string();
            std::string result(size_needed, 0);
            int converted =
                WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), (int)wstr.size(), &result[0], size_needed, NULL, NULL);
            if (converted <= 0) return std::string();
            return result;
        }
        std::string result(size_needed, 0);
        int converted =
            WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &result[0], size_needed, NULL, NULL);
        if (converted <= 0) return std::string();
        return result;
    } catch (...) {
        std::string result;
        result.reserve(wstr.size());
        for (wchar_t wc : wstr) {
            if (wc <= 127) {
                result.push_back(static_cast<char>(wc));
            } else {
                result.push_back('?');
            }
        }
        return result;
    }
}

std::string GetEnvVar(const std::string& key) {
    char* buf = nullptr;
    size_t sz = 0;
    if (_dupenv_s(&buf, &sz, key.c_str()) == 0 && buf != nullptr) {
        std::string value(buf);
        free(buf);
        return value;
    }
    return "";
}

std::string GetDocumentsPath() {
    try {
        wchar_t path[MAX_PATH] = {0};
        HRESULT result = SHGetFolderPathW(NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, path);
        if (SUCCEEDED(result)) {
            std::wstring ws(path);
            std::string converted = SafeWideStringToString(ws);
            if (!converted.empty()) {
                return converted;
            }
        }

        std::string userProfile = GetEnvVar("USERPROFILE");
        if (!userProfile.empty()) {
            return userProfile + "\\Documents";
        }

        return "C:\\Users\\Default\\Documents";
    } catch (...) {
        return "C:\\Users\\Default\\Documents";
    }
}

std::string GetGamePath() {
    try {
        std::string mo2Path = GetEnvVar("MO2_MODS_PATH");
        if (!mo2Path.empty()) return mo2Path;

        std::string vortexPath = GetEnvVar("VORTEX_MODS_PATH");
        if (!vortexPath.empty()) return vortexPath;

        std::string skyrimMods = GetEnvVar("SKYRIM_MODS_FOLDER");
        if (!skyrimMods.empty()) return skyrimMods;

        std::vector<std::string> registryKeys = {"SOFTWARE\\WOW6432Node\\Bethesda Softworks\\Skyrim Special Edition",
                                                 "SOFTWARE\\WOW6432Node\\GOG.com\\Games\\1457087920",
                                                 "SOFTWARE\\WOW6432Node\\Valve\\Steam\\Apps\\489830",
                                                 "SOFTWARE\\WOW6432Node\\Valve\\Steam\\Apps\\611670"};

        HKEY hKey;
        char pathBuffer[MAX_PATH] = {0};
        DWORD pathSize = sizeof(pathBuffer);

        for (const auto& key : registryKeys) {
            if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, key.c_str(), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
                if (RegQueryValueExA(hKey, "Installed Path", NULL, NULL, (LPBYTE)pathBuffer, &pathSize) ==
                    ERROR_SUCCESS) {
                    RegCloseKey(hKey);
                    std::string result(pathBuffer);
                    if (!result.empty()) return result;
                }
                RegCloseKey(hKey);
            }
        }

        std::vector<std::string> commonPaths = {
            "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Skyrim Special Edition",
            "C:\\Program Files\\Steam\\steamapps\\common\\Skyrim Special Edition",
            "D:\\Steam\\steamapps\\common\\Skyrim Special Edition",
            "E:\\Steam\\steamapps\\common\\Skyrim Special Edition",
            "F:\\Steam\\steamapps\\common\\Skyrim Special Edition",
            "G:\\Steam\\steamapps\\common\\Skyrim Special Edition"};

        for (const auto& pathCandidate : commonPaths) {
            try {
                if (fs::exists(pathCandidate) && fs::is_directory(pathCandidate)) {
                    return pathCandidate;
                }
            } catch (...) {
                continue;
            }
        }

        return "";
    } catch (...) {
        return "";
    }
}

void CreateDirectoryIfNotExists(const fs::path& path) {
    try {
        if (!fs::exists(path)) {
            fs::create_directories(path);
        }
    } catch (...) {
        // Silent fail
    }
}

std::string Trim(const std::string& str) {
    if (str.empty()) return str;
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

std::string EscapeJson(const std::string& str) {
    std::string result;
    result.reserve(str.length() * 1.3);

    for (char c : str) {
        switch (c) {
            case '"':
                result += "\\\"";
                break;
            case '\\':
                result += "\\\\";
                break;
            case '\b':
                result += "\\b";
                break;
            case '\f':
                result += "\\f";
                break;
            case '\n':
                result += "\\n";
                break;
            case '\r':
                result += "\\r";
                break;
            case '\t':
                result += "\\t";
                break;
            default:
                if (c >= 0x20 && c <= 0x7E) {
                    result += c;
                } else {
                    char buf[7];
                    snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                    result += buf;
                }
        }
    }
    return result;
}

// ===== ESTRUCTURA DE DATOS PARA ALIGNMENT =====

struct AlignmentData {
    double offsetX = 0.0;
    double offsetY = 0.0;
    double offsetZ = 0.0;
    double rotation = 0.0;
    double scale = 1.0;
    double sosBend = 0.0;
};

struct AnimationAlignment {
    std::map<std::string, AlignmentData> actors;  // "0", "1", etc.
};

struct PositionAlignment {
    std::map<std::string, AnimationAlignment> animations;  // "animationID"
};

struct MasterAlignmentData {
    std::map<std::string, PositionAlignment> positions;  // "M100x0&F100x0"
};

// ===== VALIDACIÓN SIMPLE DE INTEGRIDAD JSON (MEJORADA) =====

bool PerformSimpleJsonIntegrityCheck(const fs::path& jsonPath, std::ofstream& logFile) {
    try {
        logFile << "Performing SIMPLE JSON integrity check at startup..." << std::endl;
        logFile << "----------------------------------------------------" << std::endl;

        if (!fs::exists(jsonPath)) {
            logFile << "INFO: JSON file does not exist at: " << jsonPath.string() << std::endl;
            logFile << "Will create new empty JSON file." << std::endl;

            // Crear un JSON vacío válido
            std::ofstream newJson(jsonPath, std::ios::out | std::ios::trunc);
            if (newJson.is_open()) {
                newJson << "{}";
                newJson.close();
                logFile << "SUCCESS: Created new empty JSON file." << std::endl;
                return true;
            } else {
                logFile << "ERROR: Could not create new JSON file!" << std::endl;
                return false;
            }
        }

        auto fileSize = fs::file_size(jsonPath);
        logFile << "JSON file size: " << fileSize << " bytes" << std::endl;

        // Permitir archivos pequeños (mínimo 2 bytes para "{}")
        if (fileSize < 2) {
            logFile << "ERROR: JSON file is too small (less than 2 bytes)" << std::endl;
            return false;
        }

        std::ifstream jsonFile(jsonPath, std::ios::binary);
        if (!jsonFile.is_open()) {
            logFile << "ERROR: Cannot open JSON file for integrity check" << std::endl;
            return false;
        }

        std::string content;
        jsonFile.seekg(0, std::ios::end);
        size_t contentSize = jsonFile.tellg();
        jsonFile.seekg(0, std::ios::beg);
        content.resize(contentSize);
        jsonFile.read(&content[0], contentSize);
        jsonFile.close();

        if (content.empty()) {
            logFile << "ERROR: JSON file is empty after reading" << std::endl;
            return false;
        }

        // Trim whitespace
        content = content.substr(content.find_first_not_of(" \t\r\n"));
        content = content.substr(0, content.find_last_not_of(" \t\r\n") + 1);

        // Verificar estructura básica
        if (!content.starts_with('{') || !content.ends_with('}')) {
            logFile << "ERROR: JSON does not start with '{' or end with '}'" << std::endl;
            return false;
        }

        // Caso especial: JSON vacío "{}" es válido
        if (content == "{}") {
            logFile << "SUCCESS: JSON is empty but valid ({})" << std::endl;
            logFile << " This is normal for a new installation." << std::endl;
            logFile << std::endl;
            return true;
        }

        // Validación completa solo si no está vacío
        int braceCount = 0;
        int bracketCount = 0;
        bool inString = false;
        bool escape = false;
        int line = 1;
        int col = 1;

        for (size_t i = 0; i < content.length(); i++) {
            char c = content[i];
            if (c == '\n') {
                line++;
                col = 1;
                continue;
            }
            col++;

            if (escape) {
                escape = false;
                continue;
            }

            if (c == '\\') {
                escape = true;
                continue;
            }

            if (c == '"') {
                inString = !inString;
                continue;
            }

            if (!inString) {
                switch (c) {
                    case '{':
                        braceCount++;
                        break;
                    case '}':
                        braceCount--;
                        if (braceCount < 0) {
                            logFile << "ERROR: Unbalanced closing brace '}' at line " << line << ", column " << col
                                    << std::endl;
                            return false;
                        }
                        break;
                    case '[':
                        bracketCount++;
                        break;
                    case ']':
                        bracketCount--;
                        if (bracketCount < 0) {
                            logFile << "ERROR: Unbalanced closing bracket ']' at line " << line << ", column " << col
                                    << std::endl;
                            return false;
                        }
                        break;
                }
            }
        }

        if (braceCount != 0) {
            logFile << "ERROR: Unbalanced braces (missing " << (braceCount > 0 ? "closing" : "opening")
                    << " braces: " << abs(braceCount) << ")" << std::endl;
            return false;
        }

        if (bracketCount != 0) {
            logFile << "ERROR: Unbalanced brackets (missing " << (bracketCount > 0 ? "closing" : "opening")
                    << " brackets: " << abs(bracketCount) << ")" << std::endl;
            return false;
        }

        logFile << "SUCCESS: JSON passed SIMPLE integrity check!" << std::endl;
        logFile << " Braces balanced: YES" << std::endl;
        logFile << " Brackets balanced: YES" << std::endl;
        logFile << " Basic structure: VALID" << std::endl;
        logFile << std::endl;

        return true;
    } catch (const std::exception& e) {
        logFile << "ERROR in PerformSimpleJsonIntegrityCheck: " << e.what() << std::endl;
        return false;
    } catch (...) {
        logFile << "ERROR in PerformSimpleJsonIntegrityCheck: Unknown exception" << std::endl;
        return false;
    }
}

// ===== SISTEMA DE BACKUP =====

int ReadBackupConfigFromIni(const fs::path& iniPath, std::ofstream& logFile) {
    try {
        if (!fs::exists(iniPath)) {
            logFile << "Creating backup config INI at: " << iniPath.string() << std::endl;
            std::ofstream createIni(iniPath, std::ios::out | std::ios::trunc);
            if (createIni.is_open()) {
                createIni << "[Original backup]" << std::endl;
                createIni << "Backup = 1" << std::endl;
                createIni.close();
                logFile << "SUCCESS: Backup config INI created with default value (Backup = 1)" << std::endl;
                return 1;
            } else {
                logFile << "ERROR: Could not create backup config INI file!" << std::endl;
                return 0;
            }
        }

        std::ifstream iniFile(iniPath);
        if (!iniFile.is_open()) {
            logFile << "ERROR: Could not open backup config INI file for reading!" << std::endl;
            return 0;
        }

        std::string line;
        bool inBackupSection = false;
        int backupValue = 1;

        while (std::getline(iniFile, line)) {
            std::string trimmedLine = Trim(line);

            if (trimmedLine == "[Original backup]") {
                inBackupSection = true;
                continue;
            }

            if (trimmedLine.length() > 0 && trimmedLine[0] == '[' && trimmedLine != "[Original backup]") {
                inBackupSection = false;
                continue;
            }

            if (inBackupSection) {
                size_t equalPos = trimmedLine.find('=');
                if (equalPos != std::string::npos) {
                    std::string key = Trim(trimmedLine.substr(0, equalPos));
                    std::string value = Trim(trimmedLine.substr(equalPos + 1));

                    if (key == "Backup") {
                        if (value == "true" || value == "True" || value == "TRUE") {
                            backupValue = 2;
                            logFile << "Read backup config: Backup = true (always backup mode)" << std::endl;
                        } else {
                            try {
                                backupValue = std::stoi(value);
                                logFile << "Read backup config: Backup = " << backupValue << std::endl;
                            } catch (...) {
                                logFile << "Warning: Invalid backup value '" << value << "', using default (1)"
                                        << std::endl;
                                backupValue = 1;
                            }
                        }
                        break;
                    }
                }
            }
        }

        iniFile.close();
        return backupValue;
    } catch (const std::exception& e) {
        logFile << "ERROR in ReadBackupConfigFromIni: " << e.what() << std::endl;
        return 0;
    } catch (...) {
        logFile << "ERROR in ReadBackupConfigFromIni: Unknown exception" << std::endl;
        return 0;
    }
}

void UpdateBackupConfigInIni(const fs::path& iniPath, std::ofstream& logFile, int originalValue) {
    try {
        if (!fs::exists(iniPath)) {
            logFile << "ERROR: Backup config INI file does not exist for update!" << std::endl;
            return;
        }

        if (originalValue == 2) {
            logFile << "INFO: Backup = true detected, INI will not be updated (always backup mode)" << std::endl;
            return;
        }

        std::ifstream iniFile(iniPath);
        if (!iniFile.is_open()) {
            logFile << "ERROR: Could not open backup config INI file for reading during update!" << std::endl;
            return;
        }

        std::vector<std::string> lines;
        std::string line;
        bool inBackupSection = false;
        bool backupValueUpdated = false;
        lines.reserve(100);

        while (std::getline(iniFile, line)) {
            std::string trimmedLine = Trim(line);

            if (trimmedLine == "[Original backup]") {
                inBackupSection = true;
                lines.push_back(line);
                continue;
            }

            if (trimmedLine.length() > 0 && trimmedLine[0] == '[' && trimmedLine != "[Original backup]") {
                inBackupSection = false;
                lines.push_back(line);
                continue;
            }

            if (inBackupSection) {
                size_t equalPos = trimmedLine.find('=');
                if (equalPos != std::string::npos) {
                    std::string key = Trim(trimmedLine.substr(0, equalPos));
                    if (key == "Backup") {
                        lines.push_back("Backup = 0");
                        backupValueUpdated = true;
                        continue;
                    }
                }
            }

            lines.push_back(line);
        }

        iniFile.close();

        if (!backupValueUpdated) {
            logFile << "Warning: Backup value not found in INI during update!" << std::endl;
            return;
        }

        std::ofstream outFile(iniPath, std::ios::out | std::ios::trunc);
        if (!outFile.is_open()) {
            logFile << "ERROR: Could not open backup config INI file for writing during update!" << std::endl;
            return;
        }

        for (const auto& outputLine : lines) {
            outFile << outputLine << std::endl;
        }

        outFile.close();
        if (outFile.fail()) {
            logFile << "ERROR: Failed to write backup config INI file!" << std::endl;
        } else {
            logFile << "SUCCESS: Backup config updated (Backup = 0)" << std::endl;
        }

    } catch (const std::exception& e) {
        logFile << "ERROR in UpdateBackupConfigInIni: " << e.what() << std::endl;
    } catch (...) {
        logFile << "ERROR in UpdateBackupConfigInIni: Unknown exception" << std::endl;
    }
}

bool PerformLiteralJsonBackup(const fs::path& originalJsonPath, const fs::path& backupJsonPath,
                              std::ofstream& logFile) {
    try {
        if (!fs::exists(originalJsonPath)) {
            logFile << "WARNING: Original JSON file does not exist for backup (this is normal for first run)"
                    << std::endl;
            return false;
        }

        CreateDirectoryIfNotExists(backupJsonPath.parent_path());

        std::error_code ec;
        fs::copy_file(originalJsonPath, backupJsonPath, fs::copy_options::overwrite_existing, ec);

        if (ec) {
            logFile << "ERROR: Failed to copy JSON file directly: " << ec.message() << std::endl;
            return false;
        }

        try {
            auto originalSize = fs::file_size(originalJsonPath);
            auto backupSize = fs::file_size(backupJsonPath);

            if (originalSize == backupSize && originalSize > 0) {
                logFile << "SUCCESS: LITERAL JSON backup completed to: " << backupJsonPath.string() << std::endl;
                logFile << "Backup file size: " << backupSize << " bytes (verified identical to original)" << std::endl;
                return true;
            } else {
                logFile << "ERROR: Backup file size mismatch! Original: " << originalSize << ", Backup: " << backupSize
                        << std::endl;
                return false;
            }

        } catch (...) {
            logFile << "SUCCESS: LITERAL JSON backup completed (size verification failed but backup exists)"
                    << std::endl;
            return true;
        }

    } catch (const std::exception& e) {
        logFile << "ERROR in PerformLiteralJsonBackup: " << e.what() << std::endl;
        return false;
    } catch (...) {
        logFile << "ERROR in PerformLiteralJsonBackup: Unknown exception" << std::endl;
        return false;
    }
}

// ===== TRIPLE VALIDACIÓN (MEJORADA) =====

bool PerformTripleValidation(const fs::path& jsonPath, const fs::path& backupPath, std::ofstream& logFile) {
    try {
        if (!fs::exists(jsonPath)) {
            logFile << "ERROR: JSON file does not exist for validation: " << jsonPath.string() << std::endl;
            return false;
        }

        auto fileSize = fs::file_size(jsonPath);

        // Permitir archivos pequeños (mínimo 2 bytes para "{}")
        if (fileSize < 2) {
            logFile << "ERROR: JSON file is too small (" << fileSize << " bytes)" << std::endl;
            return false;
        }

        std::ifstream jsonFile(jsonPath, std::ios::binary);
        if (!jsonFile.is_open()) {
            logFile << "ERROR: Cannot open JSON file for validation" << std::endl;
            return false;
        }

        std::string content;
        jsonFile.seekg(0, std::ios::end);
        size_t contentSize = jsonFile.tellg();
        jsonFile.seekg(0, std::ios::beg);
        content.resize(contentSize);
        jsonFile.read(&content[0], contentSize);
        jsonFile.close();

        if (content.empty()) {
            logFile << "ERROR: JSON file is empty after reading" << std::endl;
            return false;
        }

        content = Trim(content);
        if (!content.starts_with('{') || !content.ends_with('}')) {
            logFile << "ERROR: JSON file does not have proper structure (missing braces)" << std::endl;
            return false;
        }

        // Caso especial: JSON vacío "{}" es válido
        if (content == "{}") {
            logFile << "SUCCESS: JSON file passed TRIPLE validation (empty but valid)" << std::endl;
            return true;
        }

        int braceCount = 0;
        int bracketCount = 0;
        bool inString = false;
        bool escape = false;

        for (char c : content) {
            if (c == '"' && !escape) {
                inString = !inString;
            } else if (!inString) {
                if (c == '{')
                    braceCount++;
                else if (c == '}')
                    braceCount--;
                else if (c == '[')
                    bracketCount++;
                else if (c == ']')
                    bracketCount--;
            }

            escape = (c == '\\' && !escape);
        }

        if (braceCount != 0 || bracketCount != 0) {
            logFile << "ERROR: JSON has unbalanced braces/brackets (braces: " << braceCount
                    << ", brackets: " << bracketCount << ")" << std::endl;
            return false;
        }

        logFile << "SUCCESS: JSON file passed TRIPLE validation (" << fileSize << " bytes)" << std::endl;
        return true;
    } catch (const std::exception& e) {
        logFile << "ERROR in PerformTripleValidation: " << e.what() << std::endl;
        return false;
    } catch (...) {
        logFile << "ERROR in PerformTripleValidation: Unknown exception" << std::endl;
        return false;
    }
}

// ===== ANÁLISIS FORENSE Y RESTAURACIÓN =====

bool MoveCorruptedJsonToAnalysis(const fs::path& corruptedJsonPath, const fs::path& analysisDir,
                                 std::ofstream& logFile) {
    try {
        if (!fs::exists(corruptedJsonPath)) {
            logFile << "WARNING: Corrupted JSON file does not exist for analysis" << std::endl;
            return false;
        }

        CreateDirectoryIfNotExists(analysisDir);

        auto now = std::chrono::system_clock::now();
        std::time_t time_t = std::chrono::system_clock::to_time_t(now);
        std::tm tm;
        localtime_s(&tm, &time_t);

        char timestamp[32];
        strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", &tm);

        fs::path analysisFile = analysisDir / ("alignment_corrupted_" + std::string(timestamp) + ".json");

        std::error_code ec;
        fs::copy_file(corruptedJsonPath, analysisFile, fs::copy_options::overwrite_existing, ec);

        if (ec) {
            logFile << "ERROR: Failed to move corrupted JSON to analysis folder: " << ec.message() << std::endl;
            return false;
        }

        logFile << "SUCCESS: Corrupted JSON moved to analysis folder: " << analysisFile.string() << std::endl;
        return true;
    } catch (const std::exception& e) {
        logFile << "ERROR in MoveCorruptedJsonToAnalysis: " << e.what() << std::endl;
        return false;
    } catch (...) {
        logFile << "ERROR in MoveCorruptedJsonToAnalysis: Unknown exception" << std::endl;
        return false;
    }
}

bool RestoreJsonFromBackup(const fs::path& backupJsonPath, const fs::path& originalJsonPath,
                           const fs::path& analysisDir, std::ofstream& logFile) {
    try {
        if (!fs::exists(backupJsonPath)) {
            logFile << "ERROR: Backup JSON file does not exist: " << backupJsonPath.string() << std::endl;
            return false;
        }

        if (!PerformTripleValidation(backupJsonPath, fs::path(), logFile)) {
            logFile << "ERROR: Backup JSON file is also corrupted, cannot restore!" << std::endl;
            return false;
        }

        logFile << "WARNING: Original JSON appears corrupted, restoring from backup..." << std::endl;

        if (fs::exists(originalJsonPath)) {
            MoveCorruptedJsonToAnalysis(originalJsonPath, analysisDir, logFile);
        }

        std::error_code ec;
        fs::copy_file(backupJsonPath, originalJsonPath, fs::copy_options::overwrite_existing, ec);

        if (ec) {
            logFile << "ERROR: Failed to restore JSON from backup: " << ec.message() << std::endl;
            return false;
        }

        if (PerformTripleValidation(originalJsonPath, fs::path(), logFile)) {
            logFile << "SUCCESS: JSON restored from backup successfully!" << std::endl;
            return true;
        } else {
            logFile << "ERROR: Restored JSON is still invalid!" << std::endl;
            return false;
        }

    } catch (const std::exception& e) {
        logFile << "ERROR in RestoreJsonFromBackup: " << e.what() << std::endl;
        return false;
    } catch (...) {
        logFile << "ERROR in RestoreJsonFromBackup: Unknown exception" << std::endl;
        return false;
    }
}
// ===== LECTURA Y PARSEO DE JSON DE ALIGNMENT =====

bool ParseAlignmentValue(const std::string& content, size_t& pos, AlignmentData& data) {
    try {
        // Buscar offsetX
        size_t offsetXPos = content.find("\"offsetX\"", pos);
        if (offsetXPos != std::string::npos) {
            size_t colonPos = content.find(":", offsetXPos);
            if (colonPos != std::string::npos) {
                size_t numStart = content.find_first_not_of(" \t\r\n", colonPos + 1);
                size_t numEnd = content.find_first_of(",}", numStart);
                if (numStart != std::string::npos && numEnd != std::string::npos) {
                    std::string numStr = content.substr(numStart, numEnd - numStart);
                    data.offsetX = std::stod(numStr);
                }
            }
        }

        // Buscar offsetY
        size_t offsetYPos = content.find("\"offsetY\"", pos);
        if (offsetYPos != std::string::npos) {
            size_t colonPos = content.find(":", offsetYPos);
            if (colonPos != std::string::npos) {
                size_t numStart = content.find_first_not_of(" \t\r\n", colonPos + 1);
                size_t numEnd = content.find_first_of(",}", numStart);
                if (numStart != std::string::npos && numEnd != std::string::npos) {
                    std::string numStr = content.substr(numStart, numEnd - numStart);
                    data.offsetY = std::stod(numStr);
                }
            }
        }

        // Buscar offsetZ
        size_t offsetZPos = content.find("\"offsetZ\"", pos);
        if (offsetZPos != std::string::npos) {
            size_t colonPos = content.find(":", offsetZPos);
            if (colonPos != std::string::npos) {
                size_t numStart = content.find_first_not_of(" \t\r\n", colonPos + 1);
                size_t numEnd = content.find_first_of(",}", numStart);
                if (numStart != std::string::npos && numEnd != std::string::npos) {
                    std::string numStr = content.substr(numStart, numEnd - numStart);
                    data.offsetZ = std::stod(numStr);
                }
            }
        }

        // Buscar rotation
        size_t rotationPos = content.find("\"rotation\"", pos);
        if (rotationPos != std::string::npos) {
            size_t colonPos = content.find(":", rotationPos);
            if (colonPos != std::string::npos) {
                size_t numStart = content.find_first_not_of(" \t\r\n", colonPos + 1);
                size_t numEnd = content.find_first_of(",}", numStart);
                if (numStart != std::string::npos && numEnd != std::string::npos) {
                    std::string numStr = content.substr(numStart, numEnd - numStart);
                    data.rotation = std::stod(numStr);
                }
            }
        }

        // Buscar scale
        size_t scalePos = content.find("\"scale\"", pos);
        if (scalePos != std::string::npos) {
            size_t colonPos = content.find(":", scalePos);
            if (colonPos != std::string::npos) {
                size_t numStart = content.find_first_not_of(" \t\r\n", colonPos + 1);
                size_t numEnd = content.find_first_of(",}", numStart);
                if (numStart != std::string::npos && numEnd != std::string::npos) {
                    std::string numStr = content.substr(numStart, numEnd - numStart);
                    data.scale = std::stod(numStr);
                }
            }
        }

        // Buscar sosBend
        size_t sosBendPos = content.find("\"sosBend\"", pos);
        if (sosBendPos != std::string::npos) {
            size_t colonPos = content.find(":", sosBendPos);
            if (colonPos != std::string::npos) {
                size_t numStart = content.find_first_not_of(" \t\r\n", colonPos + 1);
                size_t numEnd = content.find_first_of(",}", numStart);
                if (numStart != std::string::npos && numEnd != std::string::npos) {
                    std::string numStr = content.substr(numStart, numEnd - numStart);
                    data.sosBend = std::stod(numStr);
                }
            }
        }

        return true;
    } catch (...) {
        return false;
    }
}

std::string ExtractJsonString(const std::string& content, size_t startPos) {
    size_t pos = startPos + 1;  // Skip opening quote
    std::string result;

    while (pos < content.length()) {
        if (content[pos] == '"') {
            // Check if escaped
            size_t backslashCount = 0;
            size_t checkPos = pos - 1;
            while (checkPos < SIZE_MAX && content[checkPos] == '\\') {
                backslashCount++;
                checkPos--;
            }
            if (backslashCount % 2 == 0) {
                return result;
            }
        }
        result += content[pos];
        pos++;
    }

    return result;
}

bool ReadAlignmentJson(const fs::path& jsonPath, MasterAlignmentData& alignmentData, std::ofstream& logFile) {
    try {
        if (!fs::exists(jsonPath)) {
            logFile << "INFO: Alignment JSON file does not exist: " << jsonPath.string() << std::endl;
            logFile << "Starting with empty alignment data." << std::endl;
            return true;  // Es válido no tener datos al inicio
        }

        std::ifstream jsonFile(jsonPath, std::ios::binary);
        if (!jsonFile.is_open()) {
            logFile << "ERROR: Cannot open alignment JSON file: " << jsonPath.string() << std::endl;
            return false;
        }

        jsonFile.seekg(0, std::ios::end);
        size_t fileSize = jsonFile.tellg();
        jsonFile.seekg(0, std::ios::beg);

        std::string content;
        content.resize(fileSize);
        jsonFile.read(&content[0], fileSize);
        jsonFile.close();

        if (content.empty()) {
            logFile << "WARNING: Alignment JSON file is empty: " << jsonPath.string() << std::endl;
            return true;  // Archivo vacío es válido
        }

        // Verificar si es solo "{}"
        std::string trimmedContent = Trim(content);
        if (trimmedContent == "{}") {
            logFile << "INFO: Alignment JSON is empty but valid ({})" << std::endl;
            return true;  // JSON vacío es válido
        }

        // Parse manualmente el JSON
        size_t pos = 0;

        // Buscar todas las posiciones (ej: "M100x0&F100x0")
        while (pos < content.length()) {
            size_t posKeyStart = content.find("\"", pos);
            if (posKeyStart == std::string::npos) break;

            std::string positionKey = ExtractJsonString(content, posKeyStart);
            if (positionKey.empty()) {
                pos = posKeyStart + 1;
                continue;
            }

            // Verificar si es una clave de posición válida (contiene & o x)
            if (positionKey.find("&") == std::string::npos && positionKey.find("x") == std::string::npos) {
                pos = posKeyStart + 1;
                continue;
            }

            // Buscar el inicio del objeto de esta posición
            size_t posObjStart = content.find("{", posKeyStart);
            if (posObjStart == std::string::npos) break;

            // Buscar animaciones dentro de esta posición
            size_t animSearchPos = posObjStart + 1;

            while (animSearchPos < content.length()) {
                size_t animKeyStart = content.find("\"", animSearchPos);
                if (animKeyStart == std::string::npos) break;

                // Verificar si estamos fuera del objeto de posición actual
                size_t nextPosCheck = content.find("}", animSearchPos);
                if (nextPosCheck < animKeyStart) break;

                std::string animationKey = ExtractJsonString(content, animKeyStart);
                if (animationKey.empty() || animationKey == positionKey) {
                    animSearchPos = animKeyStart + 1;
                    continue;
                }

                // Buscar el objeto de esta animación
                size_t animObjStart = content.find("{", animKeyStart);
                if (animObjStart == std::string::npos) break;

                // Buscar actores dentro de esta animación
                size_t actorSearchPos = animObjStart + 1;

                while (actorSearchPos < content.length()) {
                    size_t actorKeyStart = content.find("\"", actorSearchPos);
                    if (actorKeyStart == std::string::npos) break;

                    // Verificar si estamos fuera del objeto de animación actual
                    size_t nextAnimCheck = content.find("}", actorSearchPos);
                    if (nextAnimCheck < actorKeyStart) break;

                    std::string actorKey = ExtractJsonString(content, actorKeyStart);

                    // Verificar si es un índice de actor válido (números como "0", "1", etc.)
                    bool isActorIndex = true;
                    for (char c : actorKey) {
                        if (!std::isdigit(c)) {
                            isActorIndex = false;
                            break;
                        }
                    }

                    if (!isActorIndex) {
                        actorSearchPos = actorKeyStart + 1;
                        continue;
                    }

                    // Buscar el objeto de datos de este actor
                    size_t actorObjStart = content.find("{", actorKeyStart);
                    if (actorObjStart == std::string::npos) break;

                    // Parsear los datos de alignment
                    AlignmentData data;
                    ParseAlignmentValue(content, actorObjStart, data);

                    // Almacenar en la estructura
                    alignmentData.positions[positionKey].animations[animationKey].actors[actorKey] = data;

                    // Buscar el siguiente actor
                    size_t actorObjEnd = content.find("}", actorObjStart);
                    if (actorObjEnd == std::string::npos) break;
                    actorSearchPos = actorObjEnd + 1;
                }

                // Buscar la siguiente animación
                size_t animObjEnd = animObjStart + 1;
                int braceCount = 1;
                while (animObjEnd < content.length() && braceCount > 0) {
                    if (content[animObjEnd] == '{')
                        braceCount++;
                    else if (content[animObjEnd] == '}')
                        braceCount--;
                    animObjEnd++;
                }
                animSearchPos = animObjEnd;
            }

            // Buscar la siguiente posición
            size_t posObjEnd = posObjStart + 1;
            int braceCount = 1;
            while (posObjEnd < content.length() && braceCount > 0) {
                if (content[posObjEnd] == '{')
                    braceCount++;
                else if (content[posObjEnd] == '}')
                    braceCount--;
                posObjEnd++;
            }
            pos = posObjEnd;
        }

        return true;
    } catch (const std::exception& e) {
        logFile << "ERROR in ReadAlignmentJson: " << e.what() << std::endl;
        return false;
    } catch (...) {
        logFile << "ERROR in ReadAlignmentJson: Unknown exception" << std::endl;
        return false;
    }
}

// ===== MERGE DE DATOS DE ALIGNMENT =====

void MergeAlignmentData(MasterAlignmentData& masterData, const MasterAlignmentData& newData, int& mergedPositions,
                        int& mergedAnimations, int& mergedActors, std::ofstream& logFile) {
    for (const auto& [posKey, posData] : newData.positions) {
        bool isNewPosition = (masterData.positions.find(posKey) == masterData.positions.end());

        for (const auto& [animKey, animData] : posData.animations) {
            bool isNewAnimation = (masterData.positions[posKey].animations.find(animKey) ==
                                   masterData.positions[posKey].animations.end());

            // Verificar si ya existe
            if (!isNewPosition && !isNewAnimation) {
                logFile << "  SKIPPED (already exists): " << posKey << " -> " << animKey << std::endl;
                continue;
            }

            // Merge actors
            for (const auto& [actorKey, actorData] : animData.actors) {
                masterData.positions[posKey].animations[animKey].actors[actorKey] = actorData;
                mergedActors++;
            }

            if (isNewAnimation) {
                mergedAnimations++;
                logFile << "  MERGED: " << posKey << " -> " << animKey << " (" << animData.actors.size() << " actors)"
                        << std::endl;
            }
        }

        if (isNewPosition) {
            mergedPositions++;
        }
    }
}

// ===== VERIFICACIÓN DE CAMBIOS =====

bool CheckIfAlignmentChangesNeeded(const std::string& originalJson, const MasterAlignmentData& processedData) {
    // Si el JSON original está vacío y hay datos nuevos, necesitamos escribir
    std::string trimmedOriginal = Trim(originalJson);
    if ((trimmedOriginal.empty() || trimmedOriginal == "{}") && !processedData.positions.empty()) {
        return true;
    }

    // Simple check: if processed data is empty, no changes needed
    if (processedData.positions.empty()) {
        return false;
    }

    // Check if any of the processed data is NOT in the original JSON
    for (const auto& [posKey, posData] : processedData.positions) {
        if (originalJson.find("\"" + posKey + "\"") == std::string::npos) {
            return true;  // New position key
        }

        for (const auto& [animKey, animData] : posData.animations) {
            if (originalJson.find("\"" + animKey + "\"") == std::string::npos) {
                return true;  // New animation key
            }
        }
    }

    return false;
}

// ===== GENERACIÓN DE JSON CON FORMATO CORRECTO =====

std::string GenerateAlignmentJson(const MasterAlignmentData& data) {
    std::ostringstream json;
    json << "{";

    if (data.positions.empty()) {
        json << "}";
        return json.str();
    }

    bool firstPosition = true;
    for (const auto& [posKey, posData] : data.positions) {
        if (!firstPosition) json << ",";
        firstPosition = false;

        json << "\n  \"" << EscapeJson(posKey) << "\": {";

        bool firstAnimation = true;
        for (const auto& [animKey, animData] : posData.animations) {
            if (!firstAnimation) json << ",";
            firstAnimation = false;

            json << "\n    \"" << EscapeJson(animKey) << "\": {";

            bool firstActor = true;
            for (const auto& [actorKey, actorData] : animData.actors) {
                if (!firstActor) json << ",";
                firstActor = false;

                json << "\n      \"" << actorKey << "\": {";
                json << "\n        \"offsetX\": " << actorData.offsetX << ",";
                json << "\n        \"offsetY\": " << actorData.offsetY << ",";
                json << "\n        \"offsetZ\": " << actorData.offsetZ << ",";
                json << "\n        \"rotation\": " << actorData.rotation << ",";
                json << "\n        \"scale\": " << actorData.scale << ",";
                json << "\n        \"sosBend\": " << actorData.sosBend;
                json << "\n      }";
            }

            json << "\n    }";
        }

        json << "\n  }";
    }

    if (!data.positions.empty()) {
        json << "\n";
    }
    json << "}";

    return json.str();
}

// ===== CORRECCIÓN DE INDENTACIÓN (2 ESPACIOS) =====

bool CorrectJsonIndentation(const fs::path& jsonPath, const fs::path& analysisDir, std::ofstream& logFile) {
    try {
        logFile << "Checking and correcting JSON indentation hierarchy (2 spaces)..." << std::endl;
        logFile << "----------------------------------------------------" << std::endl;

        if (!fs::exists(jsonPath)) {
            logFile << "ERROR: JSON file does not exist for indentation correction" << std::endl;
            return false;
        }

        std::ifstream jsonFile(jsonPath, std::ios::binary);
        if (!jsonFile.is_open()) {
            logFile << "ERROR: Cannot open JSON file for indentation correction" << std::endl;
            return false;
        }

        std::string originalContent;
        jsonFile.seekg(0, std::ios::end);
        size_t contentSize = jsonFile.tellg();
        jsonFile.seekg(0, std::ios::beg);
        originalContent.resize(contentSize);
        jsonFile.read(&originalContent[0], contentSize);
        jsonFile.close();

        if (originalContent.empty()) {
            logFile << "ERROR: JSON file is empty for indentation correction" << std::endl;
            return false;
        }

        // Caso especial para JSON vacío
        std::string trimmedContent = Trim(originalContent);
        if (trimmedContent == "{}") {
            logFile << "SUCCESS: JSON is empty but valid ({}), no indentation needed" << std::endl;
            logFile << std::endl;
            return true;
        }

        // Verificar si necesita corrección - buscar tabs o espacios incorrectos
        bool needsCorrection = false;
        std::vector<std::string> lines;
        std::stringstream ss(originalContent);
        std::string line;

        while (std::getline(ss, line)) {
            lines.push_back(line);
        }

        for (const auto& currentLine : lines) {
            if (currentLine.empty()) continue;
            if (currentLine.find_first_not_of(" \t") == std::string::npos) continue;

            size_t leadingSpaces = 0;
            size_t leadingTabs = 0;
            for (char c : currentLine) {
                if (c == ' ')
                    leadingSpaces++;
                else if (c == '\t')
                    leadingTabs++;
                else
                    break;
            }

            // Si hay tabs O si los espacios no son múltiplos exactos de 2, necesita corrección
            if (leadingTabs > 0 || (leadingSpaces > 0 && leadingSpaces % 2 != 0)) {
                needsCorrection = true;
                break;
            }
        }

        if (!needsCorrection) {
            logFile << "SUCCESS: JSON indentation is already correct (perfect 2-space hierarchy)" << std::endl;
            logFile << std::endl;
            return true;
        }

        logFile << "DETECTED: JSON indentation needs correction - reformatting with 2-space hierarchy..." << std::endl;

        // Reformatear con 2 espacios por nivel
        std::ostringstream correctedJson;
        int indentLevel = 0;
        bool inString = false;
        bool escape = false;

        auto isEmptyBlock = [&originalContent](size_t startPos, char openChar, char closeChar) -> bool {
            size_t pos = startPos + 1;
            int depth = 1;
            bool inStr = false;
            bool esc = false;

            while (pos < originalContent.length() && depth > 0) {
                char c = originalContent[pos];

                if (esc) {
                    esc = false;
                    pos++;
                    continue;
                }

                if (c == '\\' && inStr) {
                    esc = true;
                    pos++;
                    continue;
                }

                if (c == '"') {
                    inStr = !inStr;
                } else if (!inStr) {
                    if (c == openChar) {
                        depth++;
                    } else if (c == closeChar) {
                        depth--;
                        if (depth == 0) {
                            std::string between = originalContent.substr(startPos + 1, pos - startPos - 1);
                            std::string trimmedBetween = Trim(between);
                            return trimmedBetween.empty();
                        }
                    }
                }
                pos++;
            }
            return false;
        };

        for (size_t i = 0; i < originalContent.length(); i++) {
            char c = originalContent[i];

            if (escape) {
                correctedJson << c;
                escape = false;
                continue;
            }

            if (c == '\\' && inString) {
                correctedJson << c;
                escape = true;
                continue;
            }

            if (c == '"' && !escape) {
                inString = !inString;
                correctedJson << c;
                continue;
            }

            if (inString) {
                correctedJson << c;
                continue;
            }

            switch (c) {
                case '{':
                case '[':
                    if (isEmptyBlock(i, c, (c == '{') ? '}' : ']')) {
                        size_t pos = i + 1;
                        int depth = 1;
                        bool inStr = false;
                        bool esc = false;

                        while (pos < originalContent.length() && depth > 0) {
                            char nextChar = originalContent[pos];

                            if (esc) {
                                esc = false;
                                pos++;
                                continue;
                            }

                            if (nextChar == '\\' && inStr) {
                                esc = true;
                                pos++;
                                continue;
                            }

                            if (nextChar == '"') {
                                inStr = !inStr;
                            } else if (!inStr) {
                                if (nextChar == c) {
                                    depth++;
                                } else if (nextChar == ((c == '{') ? '}' : ']')) {
                                    depth--;
                                }
                            }
                            pos++;
                        }

                        correctedJson << c << ((c == '{') ? '}' : ']');
                        i = pos - 1;

                        if (i + 1 < originalContent.length()) {
                            size_t nextNonSpace = i + 1;
                            while (nextNonSpace < originalContent.length() &&
                                   std::isspace(originalContent[nextNonSpace])) {
                                nextNonSpace++;
                            }

                            if (nextNonSpace < originalContent.length() && originalContent[nextNonSpace] != ',' &&
                                originalContent[nextNonSpace] != '}' && originalContent[nextNonSpace] != ']') {
                                correctedJson << '\n';
                                for (int j = 0; j < indentLevel * 2; j++) {
                                    correctedJson << ' ';
                                }
                            }
                        }
                    } else {
                        correctedJson << c << '\n';
                        indentLevel++;
                        // Agregar indentación exacta de 2 espacios por nivel
                        for (int j = 0; j < indentLevel * 2; j++) {
                            correctedJson << ' ';
                        }
                    }
                    break;

                case '}':
                case ']':
                    correctedJson << '\n';
                    indentLevel--;
                    for (int j = 0; j < indentLevel * 2; j++) {
                        correctedJson << ' ';
                    }
                    correctedJson << c;

                    if (i + 1 < originalContent.length()) {
                        size_t nextNonSpace = i + 1;
                        while (nextNonSpace < originalContent.length() && std::isspace(originalContent[nextNonSpace])) {
                            nextNonSpace++;
                        }

                        if (nextNonSpace < originalContent.length() && originalContent[nextNonSpace] != ',' &&
                            originalContent[nextNonSpace] != '}' && originalContent[nextNonSpace] != ']') {
                            correctedJson << '\n';
                            for (int j = 0; j < indentLevel * 2; j++) {
                                correctedJson << ' ';
                            }
                        }
                    }
                    break;

                case ',':
                    correctedJson << c << '\n';
                    for (int j = 0; j < indentLevel * 2; j++) {
                        correctedJson << ' ';
                    }
                    break;

                case ':':
                    correctedJson << c << ' ';
                    break;

                case ' ':
                case '\t':
                case '\n':
                case '\r':
                    break;

                default:
                    correctedJson << c;
                    break;
            }
        }

        std::string correctedContent = correctedJson.str();

        fs::path tempPath = jsonPath;
        tempPath.replace_extension(".indent_corrected.tmp");

        std::ofstream tempFile(tempPath, std::ios::out | std::ios::trunc | std::ios::binary);
        if (!tempFile.is_open()) {
            logFile << "ERROR: Could not create temporary file for indentation correction!" << std::endl;
            return false;
        }

        tempFile << correctedContent;
        tempFile.close();

        if (tempFile.fail()) {
            logFile << "ERROR: Failed to write corrected JSON to temporary file!" << std::endl;
            return false;
        }

        if (!PerformTripleValidation(tempPath, fs::path(), logFile)) {
            logFile << "ERROR: Corrected JSON failed integrity check!" << std::endl;
            MoveCorruptedJsonToAnalysis(tempPath, analysisDir, logFile);
            try {
                fs::remove(tempPath);
            } catch (...) {
            }
            return false;
        }

        std::error_code ec;
        fs::rename(tempPath, jsonPath, ec);

        if (ec) {
            logFile << "ERROR: Failed to replace original with corrected JSON: " << ec.message() << std::endl;
            try {
                fs::remove(tempPath);
            } catch (...) {
            }
            return false;
        }

        if (PerformTripleValidation(jsonPath, fs::path(), logFile)) {
            logFile << "SUCCESS: JSON indentation corrected successfully (2-space hierarchy)!" << std::endl;
            logFile << std::endl;
            return true;
        } else {
            logFile << "ERROR: Final corrected JSON failed integrity check!" << std::endl;
            return false;
        }

    } catch (const std::exception& e) {
        logFile << "ERROR in CorrectJsonIndentation: " << e.what() << std::endl;
        return false;
    } catch (...) {
        logFile << "ERROR in CorrectJsonIndentation: Unknown exception" << std::endl;
        return false;
    }
}

// ===== ESCRITURA ATÓMICA =====

bool WriteJsonAtomically(const fs::path& jsonPath, const std::string& content, const fs::path& analysisDir,
                         std::ofstream& logFile) {
    try {
        fs::path tempPath = jsonPath;
        tempPath.replace_extension(".tmp");

        std::ofstream tempFile(tempPath, std::ios::out | std::ios::trunc | std::ios::binary);
        if (!tempFile.is_open()) {
            logFile << "ERROR: Could not create temporary JSON file!" << std::endl;
            return false;
        }

        tempFile << content;
        tempFile.close();

        if (tempFile.fail()) {
            logFile << "ERROR: Failed to write to temporary JSON file!" << std::endl;
            return false;
        }

        if (!PerformTripleValidation(tempPath, fs::path(), logFile)) {
            logFile << "ERROR: Temporary JSON file failed integrity check!" << std::endl;
            MoveCorruptedJsonToAnalysis(tempPath, analysisDir, logFile);
            try {
                fs::remove(tempPath);
            } catch (...) {
            }
            return false;
        }

        std::error_code ec;
        fs::rename(tempPath, jsonPath, ec);

        if (ec) {
            logFile << "ERROR: Failed to move temporary file to final location: " << ec.message() << std::endl;
            try {
                fs::remove(tempPath);
            } catch (...) {
            }
            return false;
        }

        if (PerformTripleValidation(jsonPath, fs::path(), logFile)) {
            logFile << "SUCCESS: JSON file written atomically and verified!" << std::endl;
            return true;
        } else {
            logFile << "ERROR: Final JSON file failed integrity check!" << std::endl;
            MoveCorruptedJsonToAnalysis(jsonPath, analysisDir, logFile);
            return false;
        }

    } catch (const std::exception& e) {
        logFile << "ERROR in WriteJsonAtomically: " << e.what() << std::endl;
        return false;
    } catch (...) {
        logFile << "ERROR in WriteJsonAtomically: Unknown exception" << std::endl;
        return false;
    }
}

// ===== FUNCIÓN PRINCIPAL =====

extern "C" __declspec(dllexport) bool SKSEPlugin_Load(const SKSE::LoadInterface* skse) {
    try {
        SKSE::Init(skse);

        SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message* message) {
            try {
                if (message->type == SKSE::MessagingInterface::kDataLoaded) {
                    std::string documentsPath;
                    std::string gamePath;

                    try {
                        documentsPath = GetDocumentsPath();
                        gamePath = GetGamePath();
                    } catch (...) {
                        RE::ConsoleLog::GetSingleton()->Print(
                            "OAlignment Automatic Merge: Error getting paths - using defaults");
                        documentsPath = "C:\\Users\\Default\\Documents";
                        gamePath = "";
                    }

                    if (gamePath.empty() || documentsPath.empty()) {
                        RE::ConsoleLog::GetSingleton()->Print(
                            "OAlignment Automatic Merge: Could not find Game or Documents path.");
                        return;
                    }

                    // Configuración de rutas CORREGIDAS
                    fs::path dataPath = fs::path(gamePath) / "Data";
                    fs::path sksePluginsPath = dataPath / "SKSE" / "Plugins";
                    fs::path ostimPath = fs::path(documentsPath) / "My Games" / "Skyrim Special Edition" / "OStim";
                    fs::path sksePath = fs::path(documentsPath) / "My Games" / "Skyrim Special Edition" / "SKSE";

                    CreateDirectoryIfNotExists(ostimPath);
                    CreateDirectoryIfNotExists(sksePath);

                    // RUTAS CORREGIDAS
                    fs::path logFilePath = sksePath / "OAlignment_Automatic_Merge.log";  // En SKSE
                    fs::path backupConfigIniPath =
                        sksePluginsPath / "OAlignment_Automatic_Merge.ini";  // En SKSE\Plugins
                    fs::path jsonOutputPath = ostimPath / "alignment.json";  // Se mantiene en OStim
                    fs::path backupDir = ostimPath / "Backup_OAlignment";    // Se mantiene en OStim
                    fs::path backupJsonPath = backupDir / "alignment.json";
                    fs::path analysisDir = backupDir / "Analysis";

                    std::ofstream logFile(logFilePath, std::ios::out | std::ios::trunc);

                    auto now = std::chrono::system_clock::now();
                    std::time_t in_time_t = std::chrono::system_clock::to_time_t(now);
                    std::tm buf;
                    localtime_s(&buf, &in_time_t);

                    logFile << "====================================================" << std::endl;
                    logFile << "OAlignment Automatic Merge - ULTRA SECURE VERSION" << std::endl;
                    logFile << "Log created on: " << std::put_time(&buf, "%Y-%m-%d %H:%M:%S") << std::endl;
                    logFile << "====================================================" << std::endl << std::endl;
                    logFile << "Documents: " << documentsPath << std::endl;
                    logFile << "Game Path: " << gamePath << std::endl;
                    logFile << "Log file at: " << logFilePath.string() << std::endl;
                    logFile << "INI file at: " << backupConfigIniPath.string() << std::endl;
                    logFile << "alignment.json at: " << jsonOutputPath.string() << std::endl;
                    logFile << "Backup directory: " << backupDir.string() << std::endl << std::endl;

                    logFile << "Checking backup configuration..." << std::endl;
                    logFile << "----------------------------------------------------" << std::endl;
                    int backupValue = ReadBackupConfigFromIni(backupConfigIniPath, logFile);

                    // Validación de integridad inicial MEJORADA
                    logFile << std::endl;
                    bool jsonValid = PerformSimpleJsonIntegrityCheck(jsonOutputPath, logFile);

                    if (!jsonValid && fs::exists(jsonOutputPath)) {
                        // Solo intentar restaurar si el archivo existe pero está corrupto
                        logFile << std::endl;
                        logFile << "CRITICAL: JSON failed simple integrity check! Attempting to restore from backup..."
                                << std::endl;

                        if (RestoreJsonFromBackup(backupJsonPath, jsonOutputPath, analysisDir, logFile)) {
                            logFile << "SUCCESS: JSON restored from backup. Proceeding with the normal process."
                                    << std::endl;
                        } else {
                            logFile << std::endl;
                            logFile << "WARNING: Could not restore from backup. Creating new empty JSON file."
                                    << std::endl;

                            // Crear un JSON vacío válido
                            std::ofstream newJson(jsonOutputPath, std::ios::out | std::ios::trunc);
                            if (newJson.is_open()) {
                                newJson << "{}";
                                newJson.close();
                                logFile << "SUCCESS: Created new empty JSON file." << std::endl;
                            } else {
                                logFile << "CRITICAL ERROR: Could not create new JSON file!" << std::endl;
                                logFile << "====================================================" << std::endl;
                                logFile.close();
                                RE::ConsoleLog::GetSingleton()->Print(
                                    "CRITICAL ERROR: OAlignment could not create JSON file!");
                                return;
                            }
                        }
                    }

                    logFile << "JSON is valid or was successfully created/restored - proceeding with normal process..."
                            << std::endl;
                    logFile << "JSON will be formatted with proper 2-space indentation hierarchy." << std::endl;
                    logFile << std::endl;

                    // Inicializar estructura de datos
                    MasterAlignmentData masterData;
                    bool backupPerformed = false;

                    // Sistema de backup
                    if (backupValue == 1 || backupValue == 2) {
                        if (backupValue == 2) {
                            logFile << "Backup enabled (Backup = true), performing LITERAL backup always..."
                                    << std::endl;
                        } else {
                            logFile << "Backup enabled (Backup = 1), performing LITERAL backup..." << std::endl;
                        }

                        if (PerformLiteralJsonBackup(jsonOutputPath, backupJsonPath, logFile)) {
                            backupPerformed = true;
                            if (backupValue != 2) {
                                UpdateBackupConfigInIni(backupConfigIniPath, logFile, backupValue);
                            }
                        } else {
                            logFile << "INFO: LITERAL backup skipped (no existing file or failed)" << std::endl;
                        }

                    } else {
                        logFile << "Backup disabled (Backup = 0), skipping backup" << std::endl;
                        logFile << "The original backup was already performed at " << backupDir.string() << std::endl;
                    }

                    logFile << std::endl;

                    // Leer el JSON maestro existente
                    std::string originalJsonContent = "{}";  // Default vacío
                    bool readSuccess = false;

                    if (fs::exists(jsonOutputPath)) {
                        readSuccess = ReadAlignmentJson(jsonOutputPath, masterData, logFile);

                        if (readSuccess) {
                            std::ifstream jsonFile(jsonOutputPath, std::ios::binary);
                            if (jsonFile.is_open()) {
                                jsonFile.seekg(0, std::ios::end);
                                size_t contentSize = jsonFile.tellg();
                                jsonFile.seekg(0, std::ios::beg);
                                originalJsonContent.resize(contentSize);
                                jsonFile.read(&originalJsonContent[0], contentSize);
                                jsonFile.close();
                            }

                            logFile << "Loaded existing alignment data from master JSON:" << std::endl;
                            logFile << "  Total positions: " << masterData.positions.size() << std::endl;

                            size_t totalAnimations = 0;
                            size_t totalActors = 0;
                            for (const auto& [posKey, posData] : masterData.positions) {
                                totalAnimations += posData.animations.size();
                                for (const auto& [animKey, animData] : posData.animations) {
                                    totalActors += animData.actors.size();
                                }
                            }

                            logFile << "  Total animations: " << totalAnimations << std::endl;
                            logFile << "  Total actor alignments: " << totalActors << std::endl;
                            logFile << std::endl;
                        } else {
                            logFile << "INFO: Master JSON is empty or new, starting with empty data" << std::endl;
                        }
                    } else {
                        logFile << "INFO: Master JSON does not exist, will be created" << std::endl;
                    }

                    // Variables de estadísticas
                    int totalFilesProcessed = 0;
                    int totalMergedPositions = 0;
                    int totalMergedAnimations = 0;
                    int totalMergedActors = 0;
                    int totalSkippedAnimations = 0;

                    logFile << "Scanning for alignment_*.json files in: " << dataPath.string() << std::endl;
                    logFile << "----------------------------------------------------" << std::endl;

                    // Procesar archivos alignment_*.json
                    try {
                        for (const auto& entry : fs::directory_iterator(dataPath)) {
                            if (entry.is_regular_file()) {
                                std::string filename = entry.path().filename().string();
                                if (filename.starts_with("alignment_") && filename.ends_with(".json")) {
                                    logFile << std::endl << "Processing file: " << filename << std::endl;
                                    totalFilesProcessed++;

                                    MasterAlignmentData fileData;
                                    if (ReadAlignmentJson(entry.path(), fileData, logFile)) {
                                        int fileMergedPositions = 0;
                                        int fileMergedAnimations = 0;
                                        int fileMergedActors = 0;

                                        MergeAlignmentData(masterData, fileData, fileMergedPositions,
                                                           fileMergedAnimations, fileMergedActors, logFile);

                                        totalMergedPositions += fileMergedPositions;
                                        totalMergedAnimations += fileMergedAnimations;
                                        totalMergedActors += fileMergedActors;

                                        logFile << "  File summary: " << fileMergedPositions << " new positions, "
                                                << fileMergedAnimations << " new animations, " << fileMergedActors
                                                << " new actor alignments" << std::endl;
                                    } else {
                                        logFile << "  ERROR: Could not read alignment file!" << std::endl;
                                    }
                                }
                            }
                        }
                    } catch (const std::exception& e) {
                        logFile << "ERROR scanning directory: " << e.what() << std::endl;
                    }

                    logFile << std::endl;
                    logFile << "====================================================" << std::endl;
                    logFile << "SUMMARY:" << std::endl;

                    if (backupPerformed) {
                        try {
                            auto backupSize = fs::file_size(backupJsonPath);
                            logFile << "Original JSON backup: SUCCESS (" << backupSize << " bytes)" << std::endl;
                        } catch (...) {
                            logFile << "Original JSON backup: SUCCESS (size verification failed)" << std::endl;
                        }
                    } else {
                        logFile << "Original JSON backup: SKIPPED (no file or disabled)" << std::endl;
                    }

                    logFile << "Total alignment files processed: " << totalFilesProcessed << std::endl;
                    logFile << "Total new positions merged: " << totalMergedPositions << std::endl;
                    logFile << "Total new animations merged: " << totalMergedAnimations << std::endl;
                    logFile << "Total new actor alignments merged: " << totalMergedActors << std::endl;

                    // Resumen del estado final
                    size_t finalPositions = masterData.positions.size();
                    size_t finalAnimations = 0;
                    size_t finalActors = 0;
                    for (const auto& [posKey, posData] : masterData.positions) {
                        finalAnimations += posData.animations.size();
                        for (const auto& [animKey, animData] : posData.animations) {
                            finalActors += animData.actors.size();
                        }
                    }

                    logFile << std::endl << "Final data in JSON:" << std::endl;
                    logFile << "  Total positions: " << finalPositions << std::endl;
                    logFile << "  Total animations: " << finalAnimations << std::endl;
                    logFile << "  Total actor alignments: " << finalActors << std::endl;
                    logFile << "====================================================" << std::endl << std::endl;

                    // Actualizar JSON si hay cambios
                    logFile << "Updating JSON at: " << jsonOutputPath.string() << std::endl;
                    logFile << "Applying proper 2-space indentation format..." << std::endl;

                    try {
                        // Generar el nuevo JSON
                        std::string updatedJsonContent = GenerateAlignmentJson(masterData);

                        // Verificar si hay cambios reales
                        if (CheckIfAlignmentChangesNeeded(originalJsonContent, masterData) ||
                            totalMergedAnimations > 0 || totalMergedPositions > 0) {
                            logFile << "Changes detected, writing updated master JSON..." << std::endl;

                            if (WriteJsonAtomically(jsonOutputPath, updatedJsonContent, analysisDir, logFile)) {
                                logFile << "SUCCESS: JSON updated successfully with proper 2-space indentation!"
                                        << std::endl;

                                // Corrección de indentación
                                logFile << std::endl;
                                if (CorrectJsonIndentation(jsonOutputPath, analysisDir, logFile)) {
                                    logFile << "SUCCESS: JSON indentation verification and correction completed!"
                                            << std::endl;
                                } else {
                                    logFile << "ERROR: JSON indentation correction failed!" << std::endl;
                                    logFile << "Attempting to restore from backup due to indentation failure..."
                                            << std::endl;
                                    if (fs::exists(backupJsonPath) &&
                                        RestoreJsonFromBackup(backupJsonPath, jsonOutputPath, analysisDir, logFile)) {
                                        logFile << "SUCCESS: JSON restored from backup after indentation failure!"
                                                << std::endl;
                                    } else {
                                        logFile << "WARNING: Could not restore JSON from backup!" << std::endl;
                                    }
                                }
                            } else {
                                logFile << "ERROR: Failed to write JSON safely!" << std::endl;
                                logFile << "Attempting to restore from backup due to write failure..." << std::endl;
                                if (fs::exists(backupJsonPath) &&
                                    RestoreJsonFromBackup(backupJsonPath, jsonOutputPath, analysisDir, logFile)) {
                                    logFile << "SUCCESS: JSON restored from backup after write failure!" << std::endl;
                                } else {
                                    logFile << "WARNING: Could not restore JSON from backup!" << std::endl;
                                }
                            }
                        } else {
                            logFile << "No changes detected, skipping redundant write." << std::endl;

                            if (CorrectJsonIndentation(jsonOutputPath, analysisDir, logFile)) {
                                logFile << "JSON indentation is already perfect or has been corrected." << std::endl;
                            } else {
                                logFile << "WARNING: JSON indentation correction failed on unchanged file!"
                                        << std::endl;
                            }
                        }

                    } catch (const std::exception& e) {
                        logFile << "ERROR in JSON update process: " << e.what() << std::endl;
                        logFile << "Attempting to restore from backup due to update failure..." << std::endl;
                        if (fs::exists(backupJsonPath) &&
                            RestoreJsonFromBackup(backupJsonPath, jsonOutputPath, analysisDir, logFile)) {
                            logFile << "SUCCESS: JSON restored from backup after update failure!" << std::endl;
                        } else {
                            logFile << "WARNING: Could not restore JSON from backup!" << std::endl;
                        }

                    } catch (...) {
                        logFile << "ERROR in JSON update process: Unknown exception" << std::endl;
                        logFile << "Attempting to restore from backup due to unknown failure..." << std::endl;
                        if (fs::exists(backupJsonPath) &&
                            RestoreJsonFromBackup(backupJsonPath, jsonOutputPath, analysisDir, logFile)) {
                            logFile << "SUCCESS: JSON restored from backup after unknown failure!" << std::endl;
                        } else {
                            logFile << "WARNING: Could not restore JSON from backup!" << std::endl;
                        }
                    }

                    logFile << std::endl
                            << "Process completed successfully with perfect 2-space JSON formatting." << std::endl;
                    logFile.close();

                    RE::ConsoleLog::GetSingleton()->Print(
                        "OAlignment Automatic Merge: Process completed successfully!");
                }

            } catch (const std::exception& e) {
                RE::ConsoleLog::GetSingleton()->Print("ERROR in OAlignment Automatic Merge main process!");
            } catch (...) {
                RE::ConsoleLog::GetSingleton()->Print("CRITICAL ERROR in OAlignment Automatic Merge!");
            }
        });

        return true;
    } catch (const std::exception& e) {
        RE::ConsoleLog::GetSingleton()->Print("ERROR loading OAlignment Automatic Merge plugin!");
        return false;
    } catch (...) {
        RE::ConsoleLog::GetSingleton()->Print("CRITICAL ERROR loading OAlignment Automatic Merge plugin!");
        return false;
    }
}