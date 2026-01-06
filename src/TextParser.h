#pragma once
#include <string>
#include <vector>

struct ParsedReport {
    std::wstring patientName;      // Nome paziente per il filename
    std::wstring reportBody;       // Corpo del referto estratto
    std::wstring profileUsed;      // Nome del profilo usato
    bool success;
    std::wstring errorMessage;
};

class TextParser {
public:
    // Analizza il testo grezzo del PDF e restituisce il report pulito
    static ParsedReport Parse(const std::wstring& rawText);
    
private:
    // Estrae il nome del paziente usando i pattern del profilo
    static std::wstring ExtractPatientName(const std::wstring& text, 
                                           const std::vector<std::wstring>& patterns);
    
    // Normalizza il nome paziente per uso come filename
    static std::wstring NormalizeFilename(const std::wstring& name);
    
    // Verifica se una riga deve essere esclusa
    static bool ShouldExcludeLine(const std::wstring& line,
                                  const std::vector<std::wstring>& excludePatterns,
                                  const std::vector<std::wstring>& keepPatterns);
};
