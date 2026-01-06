#include "PdfExtractor.h"
#include "Config.h"
#include <Windows.h>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <vector>

std::wstring PdfExtractor::lastError;

bool PdfExtractor::IsAvailable() {
    std::wstring pdftotextPath = Config::pdftotextPath;
    
    // Se è un percorso relativo, cerca nella directory dell'eseguibile
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

std::wstring PdfExtractor::Extract(const std::wstring& pdfPath) {
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
    // -layout mantiene il layout, -enc UTF-8 per encoding corretto
    std::wstring cmdLine = L"\"" + pdftotextPath + L"\" -layout -enc UTF-8 \"" + 
                          pdfPath + L"\" \"" + outputFile + L"\"";
    
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
