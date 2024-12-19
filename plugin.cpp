#include <RE/Skyrim.h>
#include <SKSE/API.h>
#include <SKSE/SKSE.h>
#include <shlobj.h>   // Para SHGetFolderPath
#include <windows.h>  // Para acceder al registro de Windows

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <locale>  // Para configurar el locale
#include <set>
#include <sstream>

namespace fs = std::filesystem;

std::string GetDocumentsPath() {
    wchar_t path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, path))) {
        std::wstring ws(path);
        return std::string(ws.begin(), ws.end());
    }
    return "";
}

std::string GetGamePath() {
    // Lista de posibles claves de registro para diferentes versiones del juego
    std::vector<std::string> registryKeys = {
        "SOFTWARE\\WOW6432Node\\Bethesda Softworks\\Skyrim Special Edition",
        "SOFTWARE\\WOW6432Node\\GOG.com\\Games\\1457087920",
        "SOFTWARE\\WOW6432Node\\Valve\\Steam\\Apps\\489830"  // Clave de registro para la versión de Steam
    };

    HKEY hKey;
    char path[MAX_PATH];
    DWORD pathSize = sizeof(path);

    for (const auto& key : registryKeys) {
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, key.c_str(), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            if (RegQueryValueExA(hKey, "Installed Path", NULL, NULL, (LPBYTE)path, &pathSize) == ERROR_SUCCESS) {
                RegCloseKey(hKey);
                return std::string(path);
            }
            RegCloseKey(hKey);
        }
    }

    return "";
}

void CreateDirectoryIfNotExists(const std::string& path) {
    if (!fs::exists(path)) {
        fs::create_directories(path);
    }
}

void SyncJsonFiles(const std::string& sourceFolder, const std::string& destinationFolder, std::ofstream& logFile) {
    try {
        if (!fs::exists(sourceFolder)) {
            logFile << "Source folder does not exist: " << sourceFolder << std::endl;
            return;
        }

        // Crear un conjunto de nombres de archivos JSON en la carpeta de destino
        std::set<std::string> destinationFiles;
        for (const auto& entry : fs::directory_iterator(destinationFolder)) {
            if (entry.path().extension() == ".json") {
                destinationFiles.insert(entry.path().filename().string());
            }
        }

        // Copiar o sobreescribir archivos JSON desde la carpeta de origen a la carpeta de destino
        for (const auto& entry : fs::directory_iterator(sourceFolder)) {
            if (entry.path().extension() == ".json") {
                std::string sourceFile = entry.path().string();
                std::string destinationFile =
                    (fs::path(destinationFolder) / entry.path().filename()).make_preferred().string();
                fs::copy_file(sourceFile, destinationFile, fs::copy_options::overwrite_existing);
                logFile << "Copied: " << entry.path().filename().string() << std::endl;
                destinationFiles.erase(entry.path().filename().string());
            }
        }

        // Mover archivos JSON en la carpeta de destino que no existen en la carpeta de origen
        std::string unusedCharactersFolder =
            (fs::path(destinationFolder) / "Repository of Unused Characters").make_preferred().string();
        CreateDirectoryIfNotExists(unusedCharactersFolder);

        for (const auto& fileName : destinationFiles) {
            std::string sourceFile = (fs::path(destinationFolder) / fileName).make_preferred().string();
            std::string destinationFile = (fs::path(unusedCharactersFolder) / fileName).make_preferred().string();
            fs::rename(sourceFile, destinationFile);
            logFile << "Moved to Repository: " << fileName << std::endl;
        }
    } catch (const std::filesystem::filesystem_error& e) {
        logFile << "Filesystem error: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        logFile << "General error: " << e.what() << std::endl;
    }
}

extern "C" bool SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    SKSE::Init(skse);

    // Este ejemplo imprime un arte ASCII de un gato en la consola de Skyrim y lo guarda en un archivo de log.
    SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message* message) {
        if (message->type == SKSE::MessagingInterface::kDataLoaded) {
            // 1. Obtener la ruta de la carpeta "Documentos"
            std::string documentsPath = GetDocumentsPath();

            // Crear las carpetas necesarias
            CreateDirectoryIfNotExists(documentsPath + "/My Games/Mantella/data/Skyrim/character_overrides/");
            CreateDirectoryIfNotExists(documentsPath + "/My Games/Mantella/");
            CreateDirectoryIfNotExists(documentsPath + "/My Games/Skyrim Special Edition/SKSE/");

            // Sincronizar los archivos JSON entre las carpetas de origen y destino
            std::string gamePath = GetGamePath();
            std::string sourceFolder = (fs::path(gamePath) / "Data/Mantella BackHistory").make_preferred().string();
            std::string destinationFolder =
                (fs::path(documentsPath) / "My Games/Mantella/data/Skyrim/character_overrides")
                    .make_preferred()
                    .string();

            std::string logFilePath =
                (fs::path(documentsPath) / "My Games/Mantella/Mantella-Adding-NPCs-Back-History-NG.log")
                    .make_preferred()
                    .string();
            std::ofstream logFile(logFilePath);  // Abrir el archivo en modo de sobrescritura
            if (logFile.is_open()) {
                auto now = std::chrono::system_clock::now();
                std::time_t in_time_t = std::chrono::system_clock::to_time_t(now);
                std::tm buf;
                localtime_s(&buf, &in_time_t);

                // Configurar el locale a inglés
                std::locale::global(std::locale("en_US.UTF-8"));

                logFile << "Log created on: " << std::put_time(&buf, "%Y-%m-%d %H:%M:%S") << std::endl;
                logFile << "DLL Path: " << gamePath << "SKSE\\Plugins\\Mantella-Adding-NPCs-Back-History-NG.dll"
                        << std::endl;
                logFile << "Source Folder: " << sourceFolder << std::endl;

                SyncJsonFiles(sourceFolder, destinationFolder, logFile);

                logFile.close();
            }

            // 2. Imprimir un arte ASCII de un gato en la consola de Skyrim
            const char* catArt =
                "Mantella NPCs Back History loaded successfully > ^ < .\n"
                "\n"
                "-------character_overrides-------\n"
                "";
            RE::ConsoleLog::GetSingleton()->Print(catArt);

            std::string logDestinationFile =
                (fs::path(documentsPath) /
                 "My Games/Skyrim Special Edition/SKSE/Mantella-Adding-NPCs-Back-History-NG.log")
                    .make_preferred()
                    .string();
            fs::copy_file(logFilePath, logDestinationFile, fs::copy_options::overwrite_existing);
        }
    });

    return true;
}
