#pragma once
#include <string>

class ClaudeAnalyzer {
public:
    // Verifica se Claude CLI e' disponibile (claude --version)
    static bool IsAvailable();

    // Analizza il testo del referto con Claude CLI e ritorna output strutturato
    // Ritorna stringa vuota in caso di errore (consultare GetLastError)
    static std::wstring Analyze(const std::wstring& reportText);

    // Restituisce l'ultimo messaggio di errore
    static std::wstring GetLastError();

private:
    static std::wstring lastError;
};
