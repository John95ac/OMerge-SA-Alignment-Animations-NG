#include <RE/Skyrim.h>
#include <SKSE/API.h>
#include <SKSE/SKSE.h>
#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/prettywriter.h>
#include <shlobj.h>  // Para SHGetFolderPath

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
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

void CreateJsonWithComments(const std::string& sourceFile, const std::string& destinationFile) {
    std::ifstream ifs(sourceFile);
    if (!ifs.is_open()) {
        return;
    }

    rapidjson::IStreamWrapper isw(ifs);
    rapidjson::Document doc;
    doc.ParseStream(isw);
    ifs.close();

    // Agregar un comentario al JSON
    rapidjson::Value comment;
    comment.SetString("Este es un comentario de prueba añadido al JSON.", doc.GetAllocator());
    doc.AddMember("Comentario", comment, doc.GetAllocator());

    std::ofstream ofs(destinationFile);
    if (!ofs.is_open()) {
        return;
    }

    rapidjson::OStreamWrapper osw(ofs);
    rapidjson::PrettyWriter<rapidjson::OStreamWrapper> writer(osw);
    writer.SetIndent(' ', 4);  // Establecer la indentación a 4 espacios
    doc.Accept(writer);
    ofs << std::endl << "// Este es un comentario de prueba añadido al JSON." << std::endl;
    ofs.close();
}

extern "C" bool SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    SKSE::Init(skse);

    // Este ejemplo imprime un arte ASCII de un gato en la consola de Skyrim y lo guarda en un archivo de log.
    SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message* message) {
        if (message->type == SKSE::MessagingInterface::kDataLoaded) {
            const char* catArt =
                "                        /\\_/\\  \n"
                "                       ( o.o ) \n"
                "                        > ^ <  \n"
                "OMerge loaded successfully — don't forget to stay hydrated!";

            // Imprimir en la consola de Skyrim
            RE::ConsoleLog::GetSingleton()->Print(catArt);

            // Obtener la ruta genérica de la carpeta "Documentos"
            std::string documentsPath = GetDocumentsPath();
            std::string baseFolderPath = documentsPath + "/My Games/Skyrim Special Edition/OStim";

            // Crear una nueva carpeta con la fecha y hora actuales
            auto now = std::chrono::system_clock::now();
            auto in_time_t = std::chrono::system_clock::to_time_t(now);
            std::tm buf;
            localtime_s(&buf, &in_time_t);
            std::stringstream ss;
            ss << std::put_time(&buf, "%Y-%m-%d_%H-%M-%S");
            std::string folderName = baseFolderPath + "/OMerge Alignment Animations NG Back/" + ss.str();

            fs::create_directories(folderName);

            // Guardar el arte ASCII en un archivo de log dentro de la nueva carpeta
            std::string logFilePath = folderName + "/OMerge_SA_Alignment_Animations_NG.log";
            std::ofstream logFile(logFilePath, std::ios::app);
            if (logFile.is_open()) {
                logFile << "Log creado el: " << ss.str() << std::endl;
                logFile << catArt << std::endl;
                logFile.close();
            }

            // Crear una copia del log en la carpeta SKSE
            std::string logDestinationFile =
                documentsPath + "/My Games/Skyrim Special Edition/SKSE/OMerge_SA_Alignment_Animations_NG.log";
            fs::copy_file(logFilePath, logDestinationFile, fs::copy_options::overwrite_existing);

            // Copiar el archivo alignment.json a la nueva carpeta
            std::string sourceFile = baseFolderPath + "/alignment.json";
            std::string destinationFile = folderName + "/alignment.json";
            fs::copy_file(sourceFile, destinationFile, fs::copy_options::overwrite_existing);

            // Crear un nuevo archivo JSON con comentarios
            std::string newJsonFile = baseFolderPath + "/alignmentprueba.json";
            CreateJsonWithComments(sourceFile, newJsonFile);
        }
    });

    return true;
}
