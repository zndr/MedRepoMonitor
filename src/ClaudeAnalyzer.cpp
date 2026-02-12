#include "ClaudeAnalyzer.h"
#include "Config.h"
#include <Windows.h>
#include <fstream>
#include <sstream>
#include <vector>

std::wstring ClaudeAnalyzer::lastError;

// Converte UTF-8 in wstring usando API Windows
static std::wstring Utf8ToWstring(const std::string& utf8) {
    if (utf8.empty()) return std::wstring();
    int size = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), NULL, 0);
    std::wstring result(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), &result[0], size);
    return result;
}

// Converte wstring in UTF-8
static std::string WstringToUtf8(const std::wstring& wide) {
    if (wide.empty()) return std::string();
    int size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(), NULL, 0, NULL, NULL);
    std::string result(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(), &result[0], size, NULL, NULL);
    return result;
}

// Costruisce il prompt completo con istruzioni + testo referto
static std::string BuildPrompt(const std::wstring& reportText) {
    std::string prompt =
        "Sei un assistente medico specializzato nell'analisi di referti medici italiani.\n"
        "Analizza il seguente testo estratto da un referto medico PDF e produci un output strutturato.\n\n"

        "## Regole di filtraggio\n\n"
        "ESCLUDI: header istituzionale (logo, nome ospedale, indirizzo, recapiti), "
        "footer (note legali, firma digitale, numerazione pagine, privacy), "
        "dati anagrafici paziente (nome, cognome, data nascita, codice fiscale, tessera sanitaria, ID paziente, indirizzo), "
        "metadati amministrativi (provenienza, numero ricovero, numero nosologico, codice impegnativa, medico prescrittore), "
        "quesito diagnostico/clinico, codici di classificazione (DRG, ICD-9/ICD-10, codici nomenclatore).\n\n"

        "INCLUDI: corpo del referto (descrizione esame, reperti, diagnosi, decorso clinico), "
        "conclusioni diagnostiche e raccomandazioni follow-up, "
        "terapia prescritta con posologia dettagliata, "
        "procedure eseguite e risultati.\n\n"

        "## Classificazione reperti patologici\n\n"
        "(+++) Urgente/critico - Richiede attenzione immediata (neoplasia sospetta, embolia, stenosi critica, frattura instabile, pneumotorace)\n"
        "(++) Da monitorare - Anomalia che richiede follow-up (nodulo < 1cm, lieve ipertrofia, diverticolosi, ernia discale senza deficit)\n"
        "(+) Incidentale/minore - Reperto anormale ma scarsa rilevanza clinica immediata (cisti semplici, calcificazioni minime, lipoma, osteofitosi lieve)\n\n"

        "Formato reperto: (severita) Reperto sintetico - \"Citazione dal testo originale\"\n\n"

        "## Formato output OBBLIGATORIO\n\n"
        "REPERTI PATOLOGICI SIGNIFICATIVI\n"
        "[lista reperti ordinati: (+++) prima, poi (++), poi (+)]\n"
        "[Se nessun reperto patologico: \"Nessun reperto patologico significativo rilevato.\"]\n\n"
        "________________________________________________________________________________\n\n"
        "TESTO COMPLETO DEL REFERTO\n"
        "[corpo del referto estratto, testo pulito]\n\n"
        "________________________________________________________________________________\n\n"
        "Data referto: [GG/MM/AAAA]\n"
        "Medico: [Titolo Nome Cognome]\n\n"

        "## Regole di formattazione\n\n"
        "- Testo continuo: unire le righe spezzate dal layout PDF\n"
        "- A capo solo dopo punto fermo o tra sezioni logiche\n"
        "- Preservare struttura logica originale (sezioni, paragrafi, sottotitoli)\n"
        "- Nessun markdown nel corpo del referto: solo testo pulito\n"
        "- Preservare valori numerici esattamente come riportati\n"
        "- Elenchi farmacologici: un farmaco per riga con posologia\n\n"

        "## Lettera di dimissione\n\n"
        "Se il referto e' una lettera di dimissione, preservare le sezioni nell'ordine: "
        "diagnosi alla dimissione, motivo del ricovero, anamnesi rilevante, decorso clinico, "
        "esami diagnostici, procedure/interventi, consulenze, condizioni alla dimissione, "
        "terapia alla dimissione (un farmaco per riga), indicazioni al follow-up.\n\n"

        "---\n\n"
        "TESTO DEL REFERTO DA ANALIZZARE:\n\n";

    prompt += WstringToUtf8(reportText);

    return prompt;
}

// Scrive il prompt in un file temporaneo UTF-8, ritorna il percorso
static std::wstring WriteTempPromptFile(const std::string& prompt) {
    wchar_t tempPath[MAX_PATH];
    wchar_t tempFile[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPath);
    GetTempFileNameW(tempPath, L"clp", 0, tempFile);

    // Scrivi come UTF-8
    std::ofstream file(tempFile, std::ios::binary);
    if (!file.is_open()) return L"";

    file.write(prompt.c_str(), prompt.size());
    file.close();

    return std::wstring(tempFile);
}

bool ClaudeAnalyzer::IsAvailable() {
    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi;
    std::wstring cmdLine = L"claude --version";
    std::vector<wchar_t> cmdBuffer(cmdLine.begin(), cmdLine.end());
    cmdBuffer.push_back(0);

    BOOL success = CreateProcessW(
        NULL, cmdBuffer.data(), NULL, NULL, FALSE,
        CREATE_NO_WINDOW, NULL, NULL, &si, &pi
    );

    if (!success) return false;

    WaitForSingleObject(pi.hProcess, 10000);
    DWORD exitCode;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return exitCode == 0;
}

std::wstring ClaudeAnalyzer::GetLastError() {
    return lastError;
}

std::wstring ClaudeAnalyzer::Analyze(const std::wstring& reportText) {
    lastError.clear();

    if (reportText.empty()) {
        lastError = L"Testo referto vuoto";
        return L"";
    }

    // Costruisci e scrivi il prompt su file temp
    std::string prompt = BuildPrompt(reportText);
    std::wstring promptFile = WriteTempPromptFile(prompt);
    if (promptFile.empty()) {
        lastError = L"Impossibile creare file temporaneo per il prompt";
        return L"";
    }

    // File temporaneo per l'output
    wchar_t tempPath[MAX_PATH];
    wchar_t tempFile[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPath);
    GetTempFileNameW(tempPath, L"clo", 0, tempFile);
    std::wstring outputFile = std::wstring(tempFile);

    // Comando: type prompt | claude --print > output
    std::wstring cmdLine = L"cmd /c type \"" + promptFile + L"\" | claude --print > \"" + outputFile + L"\"";

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
        lastError = L"Impossibile avviare Claude CLI";
        DeleteFileW(promptFile.c_str());
        DeleteFileW(outputFile.c_str());
        return L"";
    }

    // Attendi completamento con timeout configurabile
    DWORD waitResult = WaitForSingleObject(pi.hProcess, Config::claudeTimeoutMs);

    if (waitResult == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        DeleteFileW(promptFile.c_str());
        DeleteFileW(outputFile.c_str());
        lastError = L"Timeout analisi Claude (" + std::to_wstring(Config::claudeTimeoutMs / 1000) + L"s)";
        return L"";
    }

    DWORD exitCode;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    // Cleanup prompt temp
    DeleteFileW(promptFile.c_str());

    if (exitCode != 0) {
        lastError = L"Claude CLI ha restituito errore: " + std::to_wstring(exitCode);
        DeleteFileW(outputFile.c_str());
        return L"";
    }

    // Leggi output UTF-8
    std::ifstream file(outputFile, std::ios::binary);
    if (!file.is_open()) {
        lastError = L"Impossibile leggere output Claude";
        DeleteFileW(outputFile.c_str());
        return L"";
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();
    DeleteFileW(outputFile.c_str());

    std::wstring result = Utf8ToWstring(buffer.str());

    // Verifica output troppo breve
    if (result.size() < 50) {
        lastError = L"Output Claude troppo breve (" + std::to_wstring(result.size()) + L" caratteri)";
        return L"";
    }

    return result;
}
