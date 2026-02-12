#include "PdfExtractor.h"
#include "Config.h"
#include <Windows.h>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <iomanip>

std::wstring PdfExtractor::lastError;

bool PdfExtractor::IsAvailable() {
    std::wstring pdftotextPath = Config::pdftotextPath;

    // Se e' un percorso relativo, cerca nella directory dell'eseguibile
    if (pdftotextPath.find(L'\\') == std::wstring::npos &&
        pdftotextPath.find(L'/') == std::wstring::npos) {
        pdftotextPath = Config::GetExecutableDir() + L"\\" + pdftotextPath;
    }

    return std::filesystem::exists(pdftotextPath);
}

std::wstring PdfExtractor::GetLastError() {
    return lastError;
}

// Converte UTF-8 in wstring usando API Windows
static std::wstring Utf8ToWstring(const std::string& utf8) {
    if (utf8.empty()) return std::wstring();
    int size = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), NULL, 0);
    std::wstring result(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), &result[0], size);
    return result;
}

// Metodo privato per eseguire pdftotext con argomenti personalizzati
std::wstring PdfExtractor::ExecutePdftotext(const std::wstring& pdfPath, const std::wstring& additionalArgs) {
    lastError.clear();

    // Verifica che il file PDF esista
    if (!std::filesystem::exists(pdfPath)) {
        lastError = L"File PDF non trovato: " + pdfPath;
        return L"";
    }

    // Costruisci il percorso di pdftotext
    std::wstring pdftotextPath = Config::pdftotextPath;
    if (pdftotextPath.find(L'\\') == std::wstring::npos &&
        pdftotextPath.find(L'/') == std::wstring::npos) {
        pdftotextPath = Config::GetExecutableDir() + L"\\" + pdftotextPath;
    }

    if (!std::filesystem::exists(pdftotextPath)) {
        lastError = L"pdftotext.exe non trovato. Scaricarlo da https://www.xpdfreader.com/download.html";
        return L"";
    }

    // File temporaneo per l'output
    wchar_t tempPath[MAX_PATH];
    wchar_t tempFile[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPath);
    GetTempFileNameW(tempPath, L"mre", 0, tempFile);

    std::wstring outputFile = std::wstring(tempFile) + L".txt";
    DeleteFileW(tempFile); // Rimuovi il file temp creato da GetTempFileName

    // Costruisci il comando
    std::wstring cmdLine = L"\"" + pdftotextPath + L"\" -enc UTF-8 " + additionalArgs +
                          L" \"" + pdfPath + L"\" \"" + outputFile + L"\"";

    // Esegui pdftotext
    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi;

    std::vector<wchar_t> cmdBuffer(cmdLine.begin(), cmdLine.end());
    cmdBuffer.push_back(0);

    BOOL success = CreateProcessW(
        NULL,
        cmdBuffer.data(),
        NULL,
        NULL,
        FALSE,
        CREATE_NO_WINDOW,
        NULL,
        NULL,
        &si,
        &pi
    );

    if (!success) {
        lastError = L"Impossibile avviare pdftotext.exe";
        return L"";
    }

    // Attendi il completamento (max 30 secondi)
    WaitForSingleObject(pi.hProcess, 30000);

    DWORD exitCode;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (exitCode != 0) {
        lastError = L"pdftotext ha restituito errore: " + std::to_wstring(exitCode);
        DeleteFileW(outputFile.c_str());
        return L"";
    }

    // Leggi il file di output come bytes UTF-8
    std::ifstream file(outputFile, std::ios::binary);
    if (!file.is_open()) {
        lastError = L"Impossibile leggere il file di output";
        DeleteFileW(outputFile.c_str());
        return L"";
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();

    // Rimuovi il file temporaneo
    DeleteFileW(outputFile.c_str());

    // Converti UTF-8 in wstring
    return Utf8ToWstring(buffer.str());
}

std::wstring PdfExtractor::Extract(const std::wstring& pdfPath) {
    // Usa -layout per mantenere il layout originale
    return ExecutePdftotext(pdfPath, L"-layout");
}

std::wstring PdfExtractor::ExtractZone(const std::wstring& pdfPath, const PdfZone& zone) {
    // Costruisci gli argomenti per l'estrazione della zona
    // pdftotext usa: -x X -y Y -W width -H height -f firstPage -l lastPage
    // Le coordinate sono in punti PDF (72 punti = 1 pollice)
    // pdftotext usa origine in alto-sinistra

    std::wostringstream args;
    args << std::fixed << std::setprecision(0);
    args << L"-x " << static_cast<int>(zone.x)
         << L" -y " << static_cast<int>(zone.y)
         << L" -W " << static_cast<int>(zone.width)
         << L" -H " << static_cast<int>(zone.height);

    if (zone.page > 0) {
        args << L" -f " << zone.page << L" -l " << zone.page;
    }

    args << L" -layout";

    return ExecutePdftotext(pdfPath, args.str());
}

std::wstring PdfExtractor::ExtractZones(const std::wstring& pdfPath, const std::vector<PdfZone>& zones) {
    std::wstring result;

    for (size_t i = 0; i < zones.size(); i++) {
        std::wstring zoneText = ExtractZone(pdfPath, zones[i]);
        if (!zoneText.empty()) {
            if (!result.empty()) {
                result += L"\n";
            }
            result += zoneText;
        }
    }

    return result;
}

bool PdfExtractor::IsPythonAvailable() {
    // Verifica se Python è disponibile eseguendo "python --version"
    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi;
    std::wstring cmdLine = L"python --version";
    std::vector<wchar_t> cmdBuffer(cmdLine.begin(), cmdLine.end());
    cmdBuffer.push_back(0);

    BOOL success = CreateProcessW(
        NULL, cmdBuffer.data(), NULL, NULL, FALSE,
        CREATE_NO_WINDOW, NULL, NULL, &si, &pi
    );

    if (!success) return false;

    WaitForSingleObject(pi.hProcess, 5000);
    DWORD exitCode;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return exitCode == 0;
}

std::wstring PdfExtractor::ExtractWithPython(const std::wstring& pdfPath, const std::wstring& profilePath) {
    lastError.clear();

    // Verifica che i file esistano
    if (!std::filesystem::exists(pdfPath)) {
        lastError = L"File PDF non trovato: " + pdfPath;
        return L"";
    }
    if (!std::filesystem::exists(profilePath)) {
        lastError = L"File profilo non trovato: " + profilePath;
        return L"";
    }

    // Percorso dello script Python
    std::wstring scriptPath = Config::GetExecutableDir() + L"\\extract_zones.py";
    if (!std::filesystem::exists(scriptPath)) {
        lastError = L"Script Python non trovato: " + scriptPath;
        return L"";
    }

    // File temporaneo per l'output
    wchar_t tempPath[MAX_PATH];
    wchar_t tempFile[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPath);
    GetTempFileNameW(tempPath, L"pye", 0, tempFile);
    std::wstring outputFile = std::wstring(tempFile);

    // Costruisci il comando: python script.py pdf_path profile_path > output
    std::wstring cmdLine = L"cmd /c python \"" + scriptPath + L"\" \"" + pdfPath +
                          L"\" \"" + profilePath + L"\" > \"" + outputFile + L"\"";

    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi;
    std::vector<wchar_t> cmdBuffer(cmdLine.begin(), cmdLine.end());
    cmdBuffer.push_back(0);

    BOOL success = CreateProcessW(
        NULL, cmdBuffer.data(), NULL, NULL, FALSE,
        CREATE_NO_WINDOW, NULL, NULL, &si, &pi
    );

    if (!success) {
        lastError = L"Impossibile avviare Python";
        DeleteFileW(outputFile.c_str());
        return L"";
    }

    // Attendi completamento (max 60 secondi per PDF grandi)
    WaitForSingleObject(pi.hProcess, 60000);

    DWORD exitCode;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (exitCode != 0) {
        lastError = L"Script Python ha restituito errore: " + std::to_wstring(exitCode);
        DeleteFileW(outputFile.c_str());
        return L"";
    }

    // Leggi l'output
    std::ifstream file(outputFile, std::ios::binary);
    if (!file.is_open()) {
        lastError = L"Impossibile leggere output Python";
        DeleteFileW(outputFile.c_str());
        return L"";
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();
    DeleteFileW(outputFile.c_str());

    return Utf8ToWstring(buffer.str());
}

int PdfExtractor::GetPageCount(const std::wstring& pdfPath) {
    // pdftotext non ha un modo diretto per ottenere il numero di pagine
    // Usiamo pdfinfo se disponibile, altrimenti proviamo a estrarre e contare i form feed
    // Per ora restituiamo -1 se non disponibile

    std::wstring pdfinfoPath = Config::GetExecutableDir() + L"\\pdfinfo.exe";
    if (!std::filesystem::exists(pdfinfoPath)) {
        // Fallback: non disponibile
        return -1;
    }

    // Esegui pdfinfo per ottenere il numero di pagine
    wchar_t tempPath[MAX_PATH];
    wchar_t tempFile[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPath);
    GetTempFileNameW(tempPath, L"mpi", 0, tempFile);

    std::wstring outputFile = std::wstring(tempFile);

    std::wstring cmdLine = L"cmd /c \"\"" + pdfinfoPath + L"\" \"" + pdfPath + L"\" > \"" + outputFile + L"\"\"";

    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi;

    std::vector<wchar_t> cmdBuffer(cmdLine.begin(), cmdLine.end());
    cmdBuffer.push_back(0);

    BOOL success = CreateProcessW(
        NULL,
        cmdBuffer.data(),
        NULL,
        NULL,
        FALSE,
        CREATE_NO_WINDOW,
        NULL,
        NULL,
        &si,
        &pi
    );

    if (!success) {
        DeleteFileW(outputFile.c_str());
        return -1;
    }

    WaitForSingleObject(pi.hProcess, 10000);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    // Leggi l'output e cerca "Pages:"
    std::ifstream file(outputFile);
    if (!file.is_open()) {
        DeleteFileW(outputFile.c_str());
        return -1;
    }

    std::string line;
    int pageCount = -1;
    while (std::getline(file, line)) {
        if (line.find("Pages:") != std::string::npos) {
            size_t colonPos = line.find(':');
            if (colonPos != std::string::npos) {
                std::string numStr = line.substr(colonPos + 1);
                // Trim spaces
                size_t start = numStr.find_first_not_of(" \t");
                if (start != std::string::npos) {
                    numStr = numStr.substr(start);
                    try {
                        pageCount = std::stoi(numStr);
                    } catch (...) {}
                }
            }
            break;
        }
    }

    file.close();
    DeleteFileW(outputFile.c_str());

    return pageCount;
}
