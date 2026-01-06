#pragma once
#include <string>
#include <vector>
#include <memory>

// Struttura per definire un profilo di parsing
struct ReportProfile {
    std::wstring name;                              // Nome del profilo (es. "rx_Maugeri")
    std::vector<std::wstring> identifierPatterns;  // Pattern per identificare questo tipo di referto
    std::vector<std::wstring> patientNamePatterns; // Pattern per estrarre il nome paziente
    std::vector<std::wstring> excludePatterns;     // Pattern per righe da escludere
    std::vector<std::wstring> keepPatterns;        // Pattern per righe da mantenere sempre (es. firma medico)
    std::vector<std::wstring> newlineBeforePatterns; // Pattern prima dei quali inserire newline
};

class ProfileManager {
public:
    // Inizializza i profili disponibili
    static void Initialize();
    
    // Trova il profilo corretto per un testo
    static const ReportProfile* FindProfile(const std::wstring& text);
    
    // Restituisce il profilo di default
    static const ReportProfile* GetDefaultProfile();
    
    // Lista dei profili disponibili
    static const std::vector<ReportProfile>& GetProfiles();
    
private:
    static std::vector<ReportProfile> profiles;
    static ReportProfile defaultProfile;
    static bool initialized;
    
    // Registra i profili specifici
    static void RegisterProfiles();
    
    // Profili specifici
    static ReportProfile CreateProfile_RX_Maugeri();
    static ReportProfile CreateProfile_TSA_Maugeri();
    static ReportProfile CreateDefaultProfile();
};
