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
#include "ZoneProfile.h"
#include "ReportProfile.h"
#include "ClaudeAnalyzer.h"
#include <regex>

// Flag globale per disponibilita' Python
static bool g_pythonAvailable = false;

// Flag globale per disponibilita' Claude CLI
static bool g_claudeAvailable = false;

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
    // Normalizza newline: rimuovi \r per evitare \r\r\n su Windows
    std::wstring normalized;
    normalized.reserve(text.size());
    for (wchar_t c : text) {
        if (c != L'\r') {
            normalized += c;
        }
    }

    std::wofstream file(filePath);
    if (!file.is_open()) {
        return false;
    }

    // UTF-8 BOM per compatibilità
    file.imbue(std::locale(file.getloc(), new std::codecvt_utf8<wchar_t>));
    file << normalized;
    file.close();
    return true;
}

// Trova il profilo zone corretto per un PDF e restituisce il percorso del file JSON
std::wstring FindZoneProfilePath(const std::wstring& pdfPath, const ZoneProfile** outProfile) {
    *outProfile = nullptr;

    if (!ZoneProfileManager::HasProfiles()) {
        return L"";
    }

    // Estrai il testo per identificare il profilo
    std::wstring identText = PdfExtractor::Extract(pdfPath);
    if (identText.empty()) {
        return L"";
    }

    *outProfile = ZoneProfileManager::FindProfile(identText);
    if (!*outProfile) {
        return L"";
    }

    // Costruisci il percorso del file JSON del profilo
    std::wstring profilePath = Config::GetExecutableDir() + L"\\profile_" +
                               (*outProfile)->profileName + L".json";

    if (std::filesystem::exists(profilePath)) {
        return profilePath;
    }

    return L"";
}

// Callback quando viene rilevato un nuovo PDF
void OnNewPdf(const std::wstring& pdfPath) {
    std::wcout << std::endl;
    PrintInfo(L"Nuovo PDF rilevato: " + pdfPath);

    std::wstring rawText;
    std::wstring profileUsed = L"default";
    bool usedZoneProfile = false;

    // Prima prova con Python + profili zone se disponibili
    if (g_pythonAvailable && ZoneProfileManager::HasProfiles()) {
        const ZoneProfile* zoneProfile = nullptr;
        std::wstring profilePath = FindZoneProfilePath(pdfPath, &zoneProfile);

        if (zoneProfile && !profilePath.empty()) {
            PrintInfo(L"Profilo zone trovato: " + zoneProfile->profileName);
            PrintInfo(L"Estrazione con PyMuPDF...");
            rawText = PdfExtractor::ExtractWithPython(pdfPath, profilePath);

            if (!rawText.empty()) {
                profileUsed = L"python:" + zoneProfile->profileName;
                usedZoneProfile = true;
                PrintSuccess(L"Estrazione completata");
            } else {
                PrintWarning(L"Estrazione Python fallita: " + PdfExtractor::GetLastError());
            }
        }
    }

    // Se non c'e' Python/profilo o l'estrazione e' fallita, usa pdftotext
    if (rawText.empty()) {
        PrintInfo(L"Estrazione testo completo con pdftotext...");
        rawText = PdfExtractor::Extract(pdfPath);
    }

    if (rawText.empty()) {
        PrintError(L"Estrazione fallita: " + PdfExtractor::GetLastError());
        return;
    }

    // Se abbiamo usato Python, il testo e' gia' pulito - salta il parsing pesante
    ParsedReport report;
    if (usedZoneProfile) {
        // Il testo da Python e' gia' formattato correttamente
        report.reportBody = rawText;
        report.profileUsed = profileUsed;
        report.success = true;
        // Estrai nome paziente dal testo (semplificato)
        report.patientName = L"REFERTO";

        // Prova a estrarre il nome con i pattern del parser
        ProfileManager::Initialize();
        const ReportProfile* textProfile = ProfileManager::FindProfile(rawText);
        if (!textProfile) textProfile = ProfileManager::GetDefaultProfile();

        // Cerca pattern nome paziente
        for (const auto& pattern : textProfile->patientNamePatterns) {
            try {
                std::wregex re(pattern, std::regex::icase);
                std::wsmatch match;
                if (std::regex_search(rawText, match, re) && match.size() > 1) {
                    std::wstring name = match[1].str();
                    // Normalizza
                    std::wstring normalized;
                    bool lastWasSpace = false;
                    for (wchar_t c : name) {
                        if (iswalpha(c)) {
                            normalized += towupper(c);
                            lastWasSpace = false;
                        } else if (iswspace(c)) {
                            if (!lastWasSpace && !normalized.empty()) {
                                normalized += L'_';
                                lastWasSpace = true;
                            }
                        }
                    }
                    if (!normalized.empty() && normalized.back() == L'_') normalized.pop_back();
                    if (!normalized.empty()) {
                        report.patientName = normalized;
                        break;
                    }
                }
            } catch (...) {}
        }
    } else {
        // Usa il parser completo per pdftotext
        PrintInfo(L"Analisi del referto...");
        report = TextParser::Parse(rawText);

        if (!report.success) {
            PrintError(L"Analisi fallita: " + report.errorMessage);
            return;
        }
    }

    PrintSuccess(L"Profilo utilizzato: " + report.profileUsed);

    // Analisi AI con Claude CLI (se abilitata e disponibile)
    if (g_claudeAvailable && Config::claudeEnabled && !report.reportBody.empty()) {
        PrintInfo(L"Analisi AI con Claude in corso...");
        std::wstring enriched = ClaudeAnalyzer::Analyze(report.reportBody);
        if (!enriched.empty()) {
            report.reportBody = enriched;
            PrintSuccess(L"Analisi AI completata");
        } else {
            PrintWarning(L"Analisi AI fallita: " + ClaudeAnalyzer::GetLastError());
            PrintInfo(L"Utilizzo testo originale");
        }
    }

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

    // Analisi AI con Claude
    std::wcout << L"\nVuoi abilitare l'analisi AI dei referti con Claude? (S/N)" << std::endl;
    std::wcout << L"(Richiede Claude Code CLI installato e abbonamento Claude Max)" << std::endl;
    std::wcout << L"> ";
    std::wstring claudeChoice;
    std::getline(std::wcin, claudeChoice);

    if (claudeChoice == L"S" || claudeChoice == L"s") {
        Config::claudeEnabled = true;
        PrintSuccess(L"Analisi AI Claude abilitata");
    } else {
        Config::claudeEnabled = false;
        PrintInfo(L"Analisi AI Claude disabilitata");
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

    // Verifica Python + PyMuPDF
    g_pythonAvailable = PdfExtractor::IsPythonAvailable();
    if (g_pythonAvailable) {
        PrintSuccess(L"Python disponibile (estrazione precisione con PyMuPDF)");
    } else {
        PrintWarning(L"Python non disponibile (usando solo pdftotext)");
    }

    // Verifica Claude CLI
    if (Config::claudeEnabled) {
        g_claudeAvailable = ClaudeAnalyzer::IsAvailable();
        if (g_claudeAvailable) {
            PrintSuccess(L"Claude CLI disponibile (analisi AI attiva)");
        } else {
            PrintWarning(L"Claude CLI non disponibile (analisi AI disabilitata)");
        }
    } else {
        PrintInfo(L"Analisi AI Claude: disabilitata (ClaudeEnabled=0)");
    }

    // Carica i profili zone dalla directory dell'eseguibile
    std::wstring profilesDir = Config::GetExecutableDir();
    if (ZoneProfileManager::LoadProfiles(profilesDir)) {
        PrintSuccess(L"Profili zone caricati: " + std::to_wstring(ZoneProfileManager::GetProfiles().size()));
        for (const auto& profile : ZoneProfileManager::GetProfiles()) {
            PrintInfo(L"  - " + profile.profileName + L" (" + std::to_wstring(profile.zones.size()) + L" zone)");
        }
    } else {
        PrintWarning(L"Nessun profilo zone trovato (estrazione completa)");
    }

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
