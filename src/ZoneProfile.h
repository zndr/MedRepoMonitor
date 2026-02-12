#pragma once
#include <string>
#include <vector>
#include <variant>

// Struttura per una zona di estrazione
struct ExtractionZone {
    std::wstring label;         // Nome identificativo della zona
    double x;                   // Coordinata X (punti PDF, origine in basso-sinistra)
    double y;                   // Coordinata Y
    double width;               // Larghezza zona
    double height;              // Altezza zona
    std::vector<int> pages;     // Pagine da cui estrarre (0-indexed, vuoto = tutte)
};

// Struttura per le dimensioni della pagina
struct PageSize {
    double width;
    double height;
};

// Profilo completo per l'estrazione da zone
struct ZoneProfile {
    std::wstring profileName;               // Nome del profilo
    std::wstring pdfFile;                   // File PDF di riferimento (opzionale)
    int totalPages;                         // Numero totale di pagine (opzionale)
    PageSize pageSize;                      // Dimensioni pagina
    std::vector<ExtractionZone> zones;      // Zone di estrazione
    std::vector<std::wstring> identifierPatterns;  // Pattern per identificare questo profilo
};

// Manager per i profili basati su zone
class ZoneProfileManager {
public:
    // Carica tutti i profili JSON dalla directory specificata
    static bool LoadProfiles(const std::wstring& profilesDir);

    // Carica un singolo profilo da file
    static bool LoadProfile(const std::wstring& jsonPath);

    // Trova il profilo corretto basandosi sul testo di identificazione
    static const ZoneProfile* FindProfile(const std::wstring& identificationText);

    // Restituisce tutti i profili caricati
    static const std::vector<ZoneProfile>& GetProfiles();

    // Verifica se ci sono profili caricati
    static bool HasProfiles();

    // Restituisce l'ultimo errore
    static std::wstring GetLastError();

private:
    static std::vector<ZoneProfile> profiles;
    static std::wstring lastError;

    // Parser JSON minimale per questa struttura specifica
    static bool ParseJsonProfile(const std::string& jsonContent, ZoneProfile& profile);
};
