#include <RE/Skyrim.h>
#include <SKSE/API.h>
#include <SKSE/SKSE.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace fs = std::filesystem;

extern "C" bool SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    SKSE::Init(skse);

    // Este ejemplo imprime un arte ASCII de un gato en la consola de Skyrim y lo guarda en un archivo de log.
    SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message* message) {
        if (message->type == SKSE::MessagingInterface::kDataLoaded) {
            const char* catArt =
                "       /\\_/\\  \n"
                "      ( o.o ) \n"
                "       > ^ <  \n"
                "  Made by John95ac ";

            // Imprimir en la consola de Skyrim
            RE::ConsoleLog::GetSingleton()->Print(catArt);

            // Guardar en un archivo de log
            std::ofstream logFile(
                "C:/Users/56986/OneDrive/Documentos/My Games/Skyrim Special Edition/OStim/prueba_Ostim_generado.log",
                std::ios::app);
            if (logFile.is_open()) {
                logFile << catArt << std::endl;
                logFile.close();
            }

            // Crear una nueva carpeta con la fecha y hora actuales
            auto now = std::chrono::system_clock::now();
            auto in_time_t = std::chrono::system_clock::to_time_t(now);
            std::tm buf;
            localtime_s(&buf, &in_time_t);
            std::stringstream ss;
            ss << std::put_time(&buf, "%Y-%m-%d_%H-%M-%S");
            std::string folderName =
                "C:/Users/56986/OneDrive/Documentos/My Games/Skyrim Special Edition/OStim/OMerge Alignment Animations "
                "NG Respaldo/" +
                ss.str();

            fs::create_directories(folderName);

            // Copiar el archivo alignment.json a la nueva carpeta
            std::string sourceFile =
                "C:/Users/56986/OneDrive/Documentos/My Games/Skyrim Special Edition/OStim/alignment.json";
            std::string destinationFile = folderName + "/alignment.json";
            fs::copy_file(sourceFile, destinationFile, fs::copy_options::overwrite_existing);
        }
    });

    return true;
}
