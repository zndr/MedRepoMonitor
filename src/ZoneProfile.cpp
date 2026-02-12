#include "ZoneProfile.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <Windows.h>

std::vector<ZoneProfile> ZoneProfileManager::profiles;
std::wstring ZoneProfileManager::lastError;

// Converte UTF-8 in wstring
static std::wstring Utf8ToWstring(const std::string& utf8) {
    if (utf8.empty()) return std::wstring();
    int size = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), NULL, 0);
    std::wstring result(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), &result[0], size);
    return result;
}

// Trova il valore di una chiave JSON stringa
static std::string FindJsonString(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\"";
    size_t keyPos = json.find(searchKey);
    if (keyPos == std::string::npos) return "";

    size_t colonPos = json.find(':', keyPos + searchKey.length());
    if (colonPos == std::string::npos) return "";

    size_t startQuote = json.find('"', colonPos + 1);
    if (startQuote == std::string::npos) return "";

    size_t endQuote = json.find('"', startQuote + 1);
    if (endQuote == std::string::npos) return "";

    return json.substr(startQuote + 1, endQuote - startQuote - 1);
}

// Trova il valore di una chiave JSON numerica
static double FindJsonNumber(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\"";
    size_t keyPos = json.find(searchKey);
    if (keyPos == std::string::npos) return 0.0;

    size_t colonPos = json.find(':', keyPos + searchKey.length());
    if (colonPos == std::string::npos) return 0.0;

    // Salta spazi
    size_t valueStart = colonPos + 1;
    while (valueStart < json.length() && (json[valueStart] == ' ' || json[valueStart] == '\t' || json[valueStart] == '\n' || json[valueStart] == '\r')) {
        valueStart++;
    }

    // Leggi il numero
    std::string numStr;
    while (valueStart < json.length() && (isdigit(json[valueStart]) || json[valueStart] == '.' || json[valueStart] == '-')) {
        numStr += json[valueStart++];
    }

    return numStr.empty() ? 0.0 : std::stod(numStr);
}

// Trova un intero JSON
static int FindJsonInt(const std::string& json, const std::string& key) {
    return static_cast<int>(FindJsonNumber(json, key));
}

// Trova un oggetto JSON
static std::string FindJsonObject(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\"";
    size_t keyPos = json.find(searchKey);
    if (keyPos == std::string::npos) return "";

    size_t braceStart = json.find('{', keyPos);
    if (braceStart == std::string::npos) return "";

    int braceCount = 1;
    size_t pos = braceStart + 1;
    while (pos < json.length() && braceCount > 0) {
        if (json[pos] == '{') braceCount++;
        else if (json[pos] == '}') braceCount--;
        pos++;
    }

    return json.substr(braceStart, pos - braceStart);
}

// Trova un array JSON
static std::string FindJsonArray(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\"";
    size_t keyPos = json.find(searchKey);
    if (keyPos == std::string::npos) return "";

    size_t bracketStart = json.find('[', keyPos);
    if (bracketStart == std::string::npos) return "";

    int bracketCount = 1;
    size_t pos = bracketStart + 1;
    while (pos < json.length() && bracketCount > 0) {
        if (json[pos] == '[') bracketCount++;
        else if (json[pos] == ']') bracketCount--;
        pos++;
    }

    return json.substr(bracketStart, pos - bracketStart);
}

// Parse array di interi
static std::vector<int> ParseIntArray(const std::string& arrayStr) {
    std::vector<int> result;
    if (arrayStr.length() < 2) return result;

    std::string content = arrayStr.substr(1, arrayStr.length() - 2); // Rimuovi [ ]
    std::string numStr;

    for (char c : content) {
        if (isdigit(c) || c == '-') {
            numStr += c;
        } else if (!numStr.empty()) {
            result.push_back(std::stoi(numStr));
            numStr.clear();
        }
    }
    if (!numStr.empty()) {
        result.push_back(std::stoi(numStr));
    }

    return result;
}

// Parse array di oggetti zone
static std::vector<ExtractionZone> ParseZonesArray(const std::string& arrayStr) {
    std::vector<ExtractionZone> zones;
    if (arrayStr.length() < 2) return zones;

    size_t pos = 0;
    while (pos < arrayStr.length()) {
        size_t objStart = arrayStr.find('{', pos);
        if (objStart == std::string::npos) break;

        int braceCount = 1;
        size_t objEnd = objStart + 1;
        while (objEnd < arrayStr.length() && braceCount > 0) {
            if (arrayStr[objEnd] == '{') braceCount++;
            else if (arrayStr[objEnd] == '}') braceCount--;
            objEnd++;
        }

        std::string zoneJson = arrayStr.substr(objStart, objEnd - objStart);

        ExtractionZone zone;
        zone.label = Utf8ToWstring(FindJsonString(zoneJson, "label"));
        zone.x = FindJsonNumber(zoneJson, "x");
        zone.y = FindJsonNumber(zoneJson, "y");
        zone.width = FindJsonNumber(zoneJson, "width");
        zone.height = FindJsonNumber(zoneJson, "height");

        // Pages puo' essere un singolo intero o un array
        std::string pagesStr = FindJsonArray(zoneJson, "pages");
        if (!pagesStr.empty()) {
            zone.pages = ParseIntArray(pagesStr);
        } else {
            // Prova come singolo intero
            double pageVal = FindJsonNumber(zoneJson, "pages");
            if (pageVal >= 0) {
                zone.pages.push_back(static_cast<int>(pageVal));
            }
        }

        zones.push_back(zone);
        pos = objEnd;
    }

    return zones;
}

bool ZoneProfileManager::ParseJsonProfile(const std::string& jsonContent, ZoneProfile& profile) {
    try {
        profile.profileName = Utf8ToWstring(FindJsonString(jsonContent, "profile_name"));
        profile.pdfFile = Utf8ToWstring(FindJsonString(jsonContent, "pdf_file"));
        profile.totalPages = FindJsonInt(jsonContent, "total_pages");

        // Page size
        std::string pageSizeStr = FindJsonObject(jsonContent, "page_size");
        if (!pageSizeStr.empty()) {
            profile.pageSize.width = FindJsonNumber(pageSizeStr, "width");
            profile.pageSize.height = FindJsonNumber(pageSizeStr, "height");
        }

        // Zones
        std::string zonesStr = FindJsonArray(jsonContent, "zones");
        if (!zonesStr.empty()) {
            profile.zones = ParseZonesArray(zonesStr);
        }

        // Identifier patterns (opzionale, per ora li cerchiamo nel primo testo estratto)
        // Possono essere aggiunti successivamente al JSON se necessario

        return !profile.profileName.empty() && !profile.zones.empty();
    }
    catch (...) {
        return false;
    }
}

bool ZoneProfileManager::LoadProfiles(const std::wstring& profilesDir) {
    profiles.clear();
    lastError.clear();

    if (!std::filesystem::exists(profilesDir)) {
        lastError = L"Directory profili non trovata: " + profilesDir;
        return false;
    }

    int loadedCount = 0;
    for (const auto& entry : std::filesystem::directory_iterator(profilesDir)) {
        if (entry.is_regular_file() && entry.path().extension() == L".json") {
            std::wstring filename = entry.path().filename().wstring();
            // Carica solo file che iniziano con "profile_"
            if (filename.find(L"profile_") == 0) {
                if (LoadProfile(entry.path().wstring())) {
                    loadedCount++;
                }
            }
        }
    }

    if (loadedCount == 0) {
        lastError = L"Nessun profilo JSON valido trovato in: " + profilesDir;
        return false;
    }

    return true;
}

bool ZoneProfileManager::LoadProfile(const std::wstring& jsonPath) {
    std::ifstream file(jsonPath, std::ios::binary);
    if (!file.is_open()) {
        lastError = L"Impossibile aprire: " + jsonPath;
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();

    std::string jsonContent = buffer.str();

    ZoneProfile profile;
    if (!ParseJsonProfile(jsonContent, profile)) {
        lastError = L"Errore parsing JSON: " + jsonPath;
        return false;
    }

    profiles.push_back(profile);
    return true;
}

const ZoneProfile* ZoneProfileManager::FindProfile(const std::wstring& identificationText) {
    if (profiles.empty()) return nullptr;

    std::wstring lowerText = identificationText;
    std::transform(lowerText.begin(), lowerText.end(), lowerText.begin(), ::towlower);

    for (const auto& profile : profiles) {
        // Se il profilo ha pattern di identificazione, usali
        if (!profile.identifierPatterns.empty()) {
            bool allMatch = true;
            for (const auto& pattern : profile.identifierPatterns) {
                std::wstring lowerPattern = pattern;
                std::transform(lowerPattern.begin(), lowerPattern.end(), lowerPattern.begin(), ::towlower);
                if (lowerText.find(lowerPattern) == std::wstring::npos) {
                    allMatch = false;
                    break;
                }
            }
            if (allMatch) return &profile;
        }
        // Altrimenti cerca il nome del profilo nel testo
        else {
            std::wstring lowerName = profile.profileName;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::towlower);
            // Cerca parole chiave dal nome del profilo
            // Per ora restituisci il primo profilo (l'utente dovra' specificare i pattern)
        }
    }

    // Se non trova corrispondenza, restituisci il primo profilo come fallback
    return profiles.empty() ? nullptr : &profiles[0];
}

const std::vector<ZoneProfile>& ZoneProfileManager::GetProfiles() {
    return profiles;
}

bool ZoneProfileManager::HasProfiles() {
    return !profiles.empty();
}

std::wstring ZoneProfileManager::GetLastError() {
    return lastError;
}
