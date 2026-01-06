#include "TextParser.h"
#include "ReportProfile.h"
#include <algorithm>
#include <sstream>
#include <cwctype>
#include <regex>
#include <vector>

std::wstring TextParser::ExtractPatientName(const std::wstring& text, 
                                            const std::vector<std::wstring>& patterns) {
    for (const auto& pattern : patterns) {
        try {
            std::wregex re(pattern, std::regex::icase);
            std::wsmatch match;
            if (std::regex_search(text, match, re) && match.size() > 1) {
                std::wstring name = match[1].str();
                // Trim
                size_t start = name.find_first_not_of(L" \t\r\n");
                size_t end = name.find_last_not_of(L" \t\r\n");
                if (start != std::wstring::npos && end != std::wstring::npos) {
                    return name.substr(start, end - start + 1);
                }
            }
        } catch (...) {
            continue;
        }
    }
    return L"PAZIENTE_SCONOSCIUTO";
}

std::wstring TextParser::NormalizeFilename(const std::wstring& name) {
    std::wstring result;
    bool lastWasSpace = false;
    
    for (wchar_t c : name) {
        if (iswalpha(c)) {
            result += towupper(c);
            lastWasSpace = false;
        }
        else if (iswspace(c) || c == L',' || c == L'.') {
            if (!lastWasSpace && !result.empty()) {
                result += L'_';
                lastWasSpace = true;
            }
        }
    }
    
    if (!result.empty() && result.back() == L'_') {
        result.pop_back();
    }
    
    return result;
}

bool TextParser::ShouldExcludeLine(const std::wstring& line,
                                   const std::vector<std::wstring>& excludePatterns,
                                   const std::vector<std::wstring>& keepPatterns) {
    // Prima controlla se la riga deve essere MANTENUTA (priorità alta)
    for (const auto& pattern : keepPatterns) {
        try {
            std::wregex re(pattern, std::regex::icase);
            if (std::regex_search(line, re)) {
                return false; // NON escludere
            }
        } catch (...) {
            continue;
        }
    }
    
    // Poi controlla se deve essere esclusa
    for (const auto& pattern : excludePatterns) {
        try {
            std::wregex re(pattern, std::regex::icase);
            if (std::regex_search(line, re)) {
                return true; // Escludere
            }
        } catch (...) {
            continue;
        }
    }
    
    return false; // Default: non escludere
}

ParsedReport TextParser::Parse(const std::wstring& rawText) {
    ParsedReport result;
    result.success = false;
    
    if (rawText.empty()) {
        result.errorMessage = L"Testo vuoto";
        return result;
    }
    
    // Inizializza il profile manager
    ProfileManager::Initialize();
    
    // Trova il profilo corretto per questo referto
    const ReportProfile* profile = ProfileManager::FindProfile(rawText);
    if (!profile) {
        profile = ProfileManager::GetDefaultProfile();
    }
    result.profileUsed = profile->name;
    
    // Estrai il nome del paziente
    std::wstring patientName = ExtractPatientName(rawText, profile->patientNamePatterns);
    result.patientName = NormalizeFilename(patientName);
    
    // Dividi in righe e filtra
    std::wistringstream stream(rawText);
    std::wstring line;
    std::vector<std::wstring> outputLines;
    
    while (std::getline(stream, line)) {
        // Trim
        size_t start = line.find_first_not_of(L" \t\r\n");
        if (start == std::wstring::npos) {
            continue; // Riga vuota
        }
        size_t end = line.find_last_not_of(L" \t\r\n");
        std::wstring trimmed = line.substr(start, end - start + 1);
        
        if (trimmed.empty()) {
            continue;
        }
        
        // Verifica se escludere (passa la riga originale per il match)
        if (!ShouldExcludeLine(line, profile->excludePatterns, profile->keepPatterns)) {
            outputLines.push_back(trimmed);
        }
    }
    
    // Unisci le righe
    std::wstring body;
    for (size_t i = 0; i < outputLines.size(); i++) {
        if (i > 0) {
            body += L" ";
        }
        body += outputLines[i];
    }

    // Inserisci newline prima dei pattern specificati
    for (const auto& pattern : profile->newlineBeforePatterns) {
        try {
            std::wregex re(pattern, std::regex::icase);
            body = std::regex_replace(body, re, L"\n$&");
        } catch (...) {
            continue;
        }
    }

    // Normalizza spazi multipli (preserva newline)
    std::wstring normalized;
    bool lastWasSpace = false;
    for (wchar_t c : body) {
        if (c == L'\n') {
            // Preserva i newline, rimuovi spazi prima
            while (!normalized.empty() && normalized.back() == L' ') {
                normalized.pop_back();
            }
            normalized += L'\n';
            lastWasSpace = true;
        } else if (iswspace(c)) {
            if (!lastWasSpace) {
                normalized += L' ';
                lastWasSpace = true;
            }
        } else {
            normalized += c;
            lastWasSpace = false;
        }
    }
    
    // Trim finale
    size_t s = normalized.find_first_not_of(L" ");
    size_t e = normalized.find_last_not_of(L" ");
    if (s != std::wstring::npos && e != std::wstring::npos) {
        normalized = normalized.substr(s, e - s + 1);
    }
    
    result.reportBody = normalized;
    result.success = true;
    
    return result;
}
