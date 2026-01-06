#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING

#include <iostream>
#include <fstream>
#include <filesystem>
#include <locale>
#include <codecvt>
#include <conio.h>
#include <fcntl.h>
#include <io.h>

#include "Config.h"
#include "FileWatcher.h"
#include "PdfExtractor.h"
#include "TextParser.h"
#include "ClipboardHelper.h"

// Colori per la console
void SetConsoleColor(WORD color) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, color);
}

void PrintSuccess(const std::wstring& msg) {
    SetConsoleColor(FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    std::wcout << L"[OK] " << msg << std::endl;
    SetConsoleColor(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
}

void PrintError(const std::wstring& msg) {
    SetConsoleColor(FOREGROUND_RED | FOREGROUND_INTENSITY);
    std::wcout << L"[ERRORE] " << msg << std::endl;
    SetConsoleColor(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
}

void PrintInfo(const std::wstring& msg) {
    SetConsoleColor(FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    std::wcout << L"[INFO] " << msg << std::endl;
    SetConsoleColor(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
}

void PrintWarning(const std::wstring& msg) {
    SetConsoleColor(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    std::wcout << L"[AVVISO] " << msg << std::endl;
    SetConsoleColor(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
}

// Mostra notifica Windows
void ShowNotification(const std::wstring& title, const std::wstring& message) {
    // Usa MessageBox per la notifica
    MessageBoxW(NULL, message.c_str(), title.c_str(), MB_OK | MB_ICONINFORMATION | MB_SYSTEMMODAL);
}

// Salva il testo in un file
bool SaveToFile(const std::wstring& text, const std::wstring& filePath) {
    std::wofstream file(filePath);
    if (!file.is_open()) {
        return false;
    }
    
    // UTF-8 BOM per compatibilità
    file.imbue(std::locale(file.getloc(), new std::codecvt_utf8<wchar_t>));
    file << text;
    file.close();
    return true;
}

// Callback quando viene rilevato un nuovo PDF
void OnNewPdf(const std::wstring& pdfPath) {
    std::wcout << std::endl;
    PrintInfo(L"Nuovo PDF rilevato: " + pdfPath);
    
    // Estrai il testo dal PDF
    PrintInfo(L"Estrazione testo in corso...");
    std::wstring rawText = PdfExtractor::Extract(pdfPath);
    
    if (rawText.empty()) {
        PrintError(L"Estrazione fallita: " + PdfExtractor::GetLastError());
        return;
    }
    
    // Analizza con parser locale
    PrintInfo(L"Analisi del referto...");
    ParsedReport report = TextParser::Parse(rawText);
    
    if (!report.success) {
        PrintError(L"Analisi fallita: " + report.errorMessage);
        return;
    }
    
    PrintSuccess(L"Profilo utilizzato: " + report.profileUsed);
    
    // Copia nella clipboard
    if (ClipboardHelper::CopyToClipboard(report.reportBody)) {
        PrintSuccess(L"Testo copiato nella clipboard");
    } else {
        PrintError(L"Impossibile copiare nella clipboard: " + ClipboardHelper::GetLastError());
    }
    
    // Salva il file di backup
    std::wstring outputDir = Config::outputDirectory;
    if (outputDir.empty()) {
        outputDir = Config::watchDirectory;
    }
    
    std::wstring outputFile = outputDir + L"\\" + report.patientName + L".txt";
    
    // Se il file esiste già, aggiungi un numero
    int counter = 1;
    while (std::filesystem::exists(outputFile)) {
        outputFile = outputDir + L"\\" + report.patientName + L"_" + std::to_wstring(counter++) + L".txt";
    }
    
    if (SaveToFile(report.reportBody, outputFile)) {
        PrintSuccess(L"File salvato: " + outputFile);
    } else {
        PrintError(L"Impossibile salvare il file: " + outputFile);
    }
    
    // Mostra notifica
    std::wstring notifyMsg = L"Paziente: " + report.patientName + L"\n\n" +
                            L"Testo copiato nella clipboard.\n" +
                            L"File salvato: " + outputFile;
    ShowNotification(L"Medical Report Monitor", notifyMsg);
    
    std::wcout << std::endl;
    PrintInfo(L"In attesa di nuovi PDF... (premi Q per uscire)");
}

// Configurazione iniziale
bool RunSetup() {
    std::wcout << L"\n========================================" << std::endl;
    std::wcout << L"  MEDICAL REPORT MONITOR - Setup" << std::endl;
    std::wcout << L"========================================\n" << std::endl;
    
    // Verifica pdftotext
    if (!PdfExtractor::IsAvailable()) {
        PrintWarning(L"pdftotext.exe non trovato!");
        std::wcout << L"\nScarica Xpdf tools da: https://www.xpdfreader.com/download.html" << std::endl;
        std::wcout << L"Estrai pdftotext.exe nella stessa cartella di questo programma.\n" << std::endl;
    }
    
    // Directory da monitorare
    std::wcout << L"Inserisci la directory da monitorare per i PDF:" << std::endl;
    std::wcout << L"> ";
    std::wstring watchDir;
    std::getline(std::wcin, watchDir);
    
    if (watchDir.empty() || !std::filesystem::exists(watchDir)) {
        PrintError(L"Directory non valida");
        return false;
    }
    
    Config::watchDirectory = watchDir;
    
    // Directory output (opzionale)
    std::wcout << L"\nInserisci la directory per salvare i file .txt" << std::endl;
    std::wcout << L"(premi Invio per usare la stessa directory dei PDF):" << std::endl;
    std::wcout << L"> ";
    std::wstring outDir;
    std::getline(std::wcin, outDir);
    
    if (outDir.empty()) {
        Config::outputDirectory = watchDir;
    } else if (std::filesystem::exists(outDir)) {
        Config::outputDirectory = outDir;
    } else {
        PrintWarning(L"Directory non trovata, uso la directory dei PDF");
        Config::outputDirectory = watchDir;
    }
    
    // Autostart
    std::wcout << L"\nVuoi avviare automaticamente il programma all'avvio di Windows? (S/N)" << std::endl;
    std::wcout << L"> ";
    std::wstring autostart;
    std::getline(std::wcin, autostart);
    
    if (autostart == L"S" || autostart == L"s") {
        if (Config::SetAutoStart(true)) {
            PrintSuccess(L"Autostart configurato");
        } else {
            PrintError(L"Impossibile configurare l'autostart");
        }
    }
    
    // Salva configurazione
    if (Config::SaveConfig()) {
        PrintSuccess(L"Configurazione salvata");
        return true;
    } else {
        PrintError(L"Impossibile salvare la configurazione");
        return false;
    }
}

int wmain(int argc, wchar_t* argv[]) {
    // Imposta la console per Unicode
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    _setmode(_fileno(stdout), _O_U16TEXT);
    _setmode(_fileno(stdin), _O_U16TEXT);
    
    std::wcout << L"\n========================================" << std::endl;
    std::wcout << L"  MEDICAL REPORT MONITOR v2.0" << std::endl;
    std::wcout << L"  Estrazione testo da referti medici" << std::endl;
    std::wcout << L"  (con supporto Claude Code)" << std::endl;
    std::wcout << L"========================================\n" << std::endl;
    
    // Carica o crea la configurazione
    if (!Config::LoadConfig()) {
        PrintInfo(L"Prima esecuzione - avvio configurazione");
        if (!RunSetup()) {
            std::wcout << L"\nPremi un tasto per uscire..." << std::endl;
            _getch();
            return 1;
        }
    }
    
    // Verifica pdftotext
    if (!PdfExtractor::IsAvailable()) {
        PrintError(L"pdftotext.exe non trovato!");
        std::wcout << L"\nScarica Xpdf tools da: https://www.xpdfreader.com/download.html" << std::endl;
        std::wcout << L"Estrai pdftotext.exe nella cartella: " << Config::GetExecutableDir() << std::endl;
        std::wcout << L"\nPremi un tasto per uscire..." << std::endl;
        _getch();
        return 1;
    }
    
    PrintSuccess(L"pdftotext.exe trovato");
    PrintSuccess(L"Parser locale attivo");
    
    PrintInfo(L"Directory monitorata: " + Config::watchDirectory);
    PrintInfo(L"Directory output: " + Config::outputDirectory);
    
    if (Config::IsAutoStartEnabled()) {
        PrintInfo(L"Autostart: abilitato");
    }
    
    // Avvia il file watcher
    FileWatcher watcher;
    watcher.SetCallback(OnNewPdf);
    
    if (!watcher.Start(Config::watchDirectory)) {
        PrintError(L"Impossibile avviare il monitoraggio: " + watcher.GetLastError());
        std::wcout << L"\nPremi un tasto per uscire..." << std::endl;
        _getch();
        return 1;
    }
    
    PrintSuccess(L"Monitoraggio avviato");
    std::wcout << std::endl;
    PrintInfo(L"In attesa di nuovi PDF... (premi Q per uscire)");
    std::wcout << std::endl;
    
    // Loop principale - attendi Q per uscire
    while (true) {
        if (_kbhit()) {
            int ch = _getch();
            if (ch == 'q' || ch == 'Q') {
                break;
            }
        }
        Sleep(100);
    }
    
    PrintInfo(L"Arresto in corso...");
    watcher.Stop();
    PrintSuccess(L"Programma terminato");
    
    return 0;
}
