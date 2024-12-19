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

void CopyOAlignment(const std::string& sourceFolder, const std::string& destinationFolder, std::ofstream& logFile) {
    try {
        if (!fs::exists(sourceFolder)) {
            logFile << "Source folder does not exist: " << sourceFolder << std::endl;
            return;
        }

        for (const auto& modderEntry : fs::directory_iterator(sourceFolder)) {
            if (modderEntry.is_directory()) {
                std::string modderName = modderEntry.path().filename().string();
                logFile << modderName << std::endl;

                for (const auto& packageEntry : fs::directory_iterator(modderEntry.path())) {
                    if (packageEntry.is_directory()) {
                        std::string packageName = packageEntry.path().filename().string();
                        logFile << "  " << packageName << std::endl;

                        std::string sourceFile = (packageEntry.path() / "alignment.json").string();
                        std::string destinationFile =
                            (fs::path(destinationFolder) / modderName / packageName / "alignment.json").string();
                        CreateDirectoryIfNotExists(fs::path(destinationFile).parent_path().string());
                        fs::copy_file(fs::path(sourceFile), fs::path(destinationFile),
                                      fs::copy_options::overwrite_existing);

                        // Convertir alignment.json a alignment.txt
                        std::ifstream ifs(sourceFile);
                        if (ifs.is_open()) {
                            std::string txtFile =
                                (fs::path(destinationFolder) / modderName / packageName / "alignment.txt").string();
                            std::ofstream ofs(txtFile);
                            if (ofs.is_open()) {
                                ofs << ifs.rdbuf();
                                ofs.close();
                            }
                            ifs.close();
                        }
                    }
                }
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        logFile << "Filesystem error: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        logFile << "General error: " << e.what() << std::endl;
    }
}

void MergeAlignmentTxtFiles(const std::string& sourceFolder, const std::string& destinationFile) {
    std::ofstream ofs(destinationFile);
    if (!ofs.is_open()) {
        return;
    }

    for (const auto& modderEntry : fs::directory_iterator(sourceFolder)) {
        if (modderEntry.is_directory()) {
            for (const auto& packageEntry : fs::directory_iterator(modderEntry.path())) {
                if (packageEntry.is_directory()) {
                    std::string txtFile = (packageEntry.path() / "alignment.txt").string();
                    std::ifstream ifs(txtFile);
                    if (ifs.is_open()) {
                        ofs << ifs.rdbuf() << std::endl;
                        ifs.close();
                    }
                }
            }
        }
    }

    ofs.close();
}

void CopyAlignmentJsonToOstimOriginal(const std::string& sourceFolder, const std::string& destinationFolder,
                                      std::ofstream& logFile) {
    std::string sourceFile = (fs::path(sourceFolder) / "alignment.json").string();
    std::string destinationFile = (fs::path(destinationFolder) / "alignment.json").string();

    if (fs::exists(sourceFile)) {
        CreateDirectoryIfNotExists(fs::path(destinationFile).parent_path().string());
        fs::copy_file(fs::path(sourceFile), fs::path(destinationFile), fs::copy_options::overwrite_existing);

        // Convertir alignment.json a alignment.txt
        std::ifstream ifs(sourceFile);
        if (ifs.is_open()) {
            std::string txtFile = (fs::path(destinationFolder) / "alignment.txt").string();
            std::ofstream ofs(txtFile);
            if (ofs.is_open()) {
                ofs << ifs.rdbuf();
                ofs.close();
            }
            ifs.close();
        }
    } else {
        logFile << "No alignment.json found in OStim folder." << std::endl;
    }
}

extern "C" bool SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    SKSE::Init(skse);

    SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message* message) {
        if (message->type == SKSE::MessagingInterface::kDataLoaded) {
            std::string documentsPath = GetDocumentsPath();

            // Crear las carpetas necesarias
            CreateDirectoryIfNotExists(
                documentsPath + "\\My Games\\Skyrim Special Edition\\OStim\\OMerge Alignment Animations NG Back");
            CreateDirectoryIfNotExists(documentsPath + "\\My Games\\Skyrim Special Edition\\SKSE\\");
            CreateDirectoryIfNotExists(
                documentsPath +
                "\\My Games\\Skyrim Special Edition\\OStim\\OMerge Alignment Animations NG Back\\Ostim Original");

            // Sincronizar los archivos JSON entre las carpetas de origen y destino
            std::string gamePath = GetGamePath();
            std::string sourceFolder = (fs::path(gamePath) / "Data\\OAlignment").string();
            std::string destinationFolder =
                (fs::path(documentsPath) /
                 "My Games\\Skyrim Special Edition\\OStim\\OMerge Alignment Animations NG Back")
                    .string();

            std::string logFilePath = (fs::path(destinationFolder) / "OMerge_SA_Alignment_Animations_NG.log").string();
            std::ofstream logFile(logFilePath);  // Abrir el archivo en modo de sobrescritura
            if (logFile.is_open()) {
                auto now = std::chrono::system_clock::now();
                std::time_t in_time_t = std::chrono::system_clock::to_time_t(now);
                std::tm buf;
                localtime_s(&buf, &in_time_t);

                // Configurar el locale a inglés
                std::locale::global(std::locale("en_US.UTF-8"));

                logFile << "Log created on: " << std::put_time(&buf, "%Y-%m-%d %H:%M:%S") << std::endl;
                logFile << std::endl;

                CopyOAlignment(sourceFolder, destinationFolder, logFile);
                CopyAlignmentJsonToOstimOriginal(
                    (fs::path(documentsPath) / "My Games\\Skyrim Special Edition\\OStim").string(),
                    (fs::path(destinationFolder) / "Ostim Original").string(), logFile);

                logFile.close();
            }

            // Copiar el archivo de log a la carpeta SKSE
            std::string logDestinationFile =
                (fs::path(documentsPath) /
                 "My Games\\Skyrim Special Edition\\SKSE\\OMerge_SA_Alignment_Animations_NG.log")
                    .string();
            fs::copy_file(fs::path(logFilePath), fs::path(logDestinationFile), fs::copy_options::overwrite_existing);

            // Merge de todos los alignment.txt en un solo archivo alignment.txt en la carpeta OMerge Alignment
            // Animations NG Back
            std::string mergedTxtFile = (fs::path(destinationFolder) / "alignment.txt").string();
            MergeAlignmentTxtFiles(destinationFolder, mergedTxtFile);

                       // Copiar el archivo alignment.txt mergeado a la carpeta OStim y convertirlo a alignment.json
            std::string ostimAlignmentTxtFile =
                (fs::path(documentsPath) / "My Games\\Skyrim Special Edition\\OStim\\alignment.txt").string();
            fs::copy_file(fs::path(mergedTxtFile), fs::path(ostimAlignmentTxtFile),
                          fs::copy_options::overwrite_existing);

            // Convertir alignment.txt a alignment.json
            std::ifstream ifs(ostimAlignmentTxtFile);
            if (ifs.is_open()) {
                std::string jsonFile =
                    (fs::path(documentsPath) / "My Games\\Skyrim Special Edition\\OStim\\alignment.json").string();
                std::ofstream ofs(jsonFile);
                if (ofs.is_open()) {
                    ofs << ifs.rdbuf();
                    ofs.close();
                }
                ifs.close();
            }

            // Imprimir un arte ASCII de un gato en la consola de Skyrim
            const char* catArt =
                "                        /\\_/\\  \n"
                "                       ( o.o ) \n"
                "                        > ^ <  \n"
                "OMerge loaded successfully. don't forget to stay hydrated!";
            RE::ConsoleLog::GetSingleton()->Print(catArt);
        }
    });

    return true;
}
