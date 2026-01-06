#pragma once
#include <string>
#include <Windows.h>

namespace Config {
    // Directory da monitorare (da configurare al primo avvio)
    inline std::wstring watchDirectory = L"";
    
    // Directory di output per i file .txt
    inline std::wstring outputDirectory = L"";
    
    // Percorso di pdftotext.exe
    inline std::wstring pdftotextPath = L"pdftotext.exe";
    
    // Nome applicazione per registro autostart
    inline const wchar_t* APP_NAME = L"MedicalReportMonitor";
    
    // File di configurazione
    inline const wchar_t* CONFIG_FILE = L"config.ini";
    
    // Funzioni di utilità
    std::wstring GetExecutableDir();
    bool LoadConfig();
    bool SaveConfig();
    bool SetAutoStart(bool enable);
    bool IsAutoStartEnabled();
}
