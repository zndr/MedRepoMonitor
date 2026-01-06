#include "ReportProfile.h"
#include <regex>
#include <algorithm>

std::vector<ReportProfile> ProfileManager::profiles;
ReportProfile ProfileManager::defaultProfile;
bool ProfileManager::initialized = false;

void ProfileManager::Initialize() {
    if (initialized) return;
    
    RegisterProfiles();
    defaultProfile = CreateDefaultProfile();
    initialized = true;
}

void ProfileManager::RegisterProfiles() {
    profiles.clear();
    profiles.push_back(CreateProfile_RX_Maugeri());
    profiles.push_back(CreateProfile_TSA_Maugeri());
}

const ReportProfile* ProfileManager::FindProfile(const std::wstring& text) {
    if (!initialized) Initialize();
    
    for (const auto& profile : profiles) {
        bool allMatch = true;
        for (const auto& pattern : profile.identifierPatterns) {
            std::wstring lowerText = text;
            std::wstring lowerPattern = pattern;
            std::transform(lowerText.begin(), lowerText.end(), lowerText.begin(), ::towlower);
            std::transform(lowerPattern.begin(), lowerPattern.end(), lowerPattern.begin(), ::towlower);
            
            if (lowerText.find(lowerPattern) == std::wstring::npos) {
                allMatch = false;
                break;
            }
        }
        if (allMatch) {
            return &profile;
        }
    }
    return nullptr;
}

const ReportProfile* ProfileManager::GetDefaultProfile() {
    if (!initialized) Initialize();
    return &defaultProfile;
}

const std::vector<ReportProfile>& ProfileManager::GetProfiles() {
    if (!initialized) Initialize();
    return profiles;
}

// ============================================================================
// PROFILO: rx_Maugeri
// ============================================================================
ReportProfile ProfileManager::CreateProfile_RX_Maugeri() {
    ReportProfile p;
    p.name = L"rx_Maugeri";
    
    p.identifierPatterns = {
        L"Istituto Scientifico di Lumezzane",
        L"Servizio di Diagnostica per Immagini"
    };
    
    p.patientNamePatterns = {
        L"Sig\\./Sig\\.ra:\\s+([A-Za-z][A-Za-z\\s]+?)(?:\\s{2,}|ID\\s+Paziente)"
    };
    
    p.excludePatterns = {
        L"Istituto\\s+Scientifico",
        L"Servizio\\s+di\\s+Diagnostica",
        L"Primario:",
        L"Tel\\.",
        L"Fax\\.",
        L"Email:",
        L"Sig\\./Sig\\.ra:",
        L"Data\\s+di\\s+Nascita:",
        L"\\d{2}/\\d{2}/\\d{4}",
        L"Codice\\s+Fiscale:",
        L"[A-Z]{6}\\d{2}[A-Z]\\d{2}[A-Z]\\d{3}[A-Z]",
        L"ID\\s+Paziente:",
        L"PK-\\d+",
        L"N\\.\\s+di\\s+accesso:",
        L"\\d{10}",
        L"Provenienza:",
        L"ESTERNO",
        L"Prestazione\\s+eseguita:",
        L"Schedulazione:",
        L"Esecuzione:",
        L"Classe\\s+dose:",
        L"Data\\s+validazione",
        L"Documento\\s+informatico",
        L"stampa\\s+costituisce",
        L"D\\.Lgs",
        L"Pag\\s+\\d+\\s+di\\s+\\d+",
        L"TSRM:",
    };
    
    p.keepPatterns = {
        L"Medico\\s+Radiologo:",
    };
    
    return p;
}

// ============================================================================
// PROFILO: tsa_maugeri
// Per referti Ecocolordoppler Tronchi Sovraortici (TSA) Maugeri
// ============================================================================
ReportProfile ProfileManager::CreateProfile_TSA_Maugeri() {
    ReportProfile p;
    p.name = L"tsa_maugeri";

    p.identifierPatterns = {
        L"ECOCOLORDOPPLER TRONCHI SOVRAORTICI"
    };

    p.patientNamePatterns = {
        L"Paziente:\\s+([A-Z]+\\s+[A-Z]+)\\s+Anni:"
    };

    p.excludePatterns = {
        // Intestazione istituto
        L"Istituti\\s+Clinici\\s+Scientifici\\s+Maugeri",
        L"Via\\s+Salvatore\\s+Maugeri",
        L"C\\.F\\.\\s+e\\s+P\\.IVA",
        L"Iscrizione\\s+Rea:",
        L"Istituto\\s+Scientifico\\s+di\\s+Lumezzane",
        L"UO\\s+RIABILITAZIONE",
        L"Dirigente\\s+Responsabile:",
        L"Via\\s+Mazzini",
        L"25065\\s+Lumezzane",
        L"Tel\\s+030",
        L"URP\\s+030",
        L"E-mail:",
        L"lumezzane@icsmaugeri",
        L"Ambulatorio,\\s+LU",
        // Luogo e data
        L"Lumezzane,\\s+\\d{2}/\\d{2}/\\d{4}",
        // Dati paziente
        L"Paziente:",
        L"Data\\s+di\\s+Nascita:",
        L"Anni:\\s+\\d+",
        L"Sesso:\\s+Maschio",
        L"Sesso:\\s+Femmina",
        L"Codice\\s+Paz\\.\\s+ID:",
        L"PK-\\d+",
        L"Indirizzo:",
        L"Citt[aà]:",
        L"Telefono:",
        L"\\d{10}",
        L"C\\.F\\.:",
        L"[A-Z]{6}\\d{2}[A-Z]\\d{2}[A-Z]\\d{3}[A-Z]",
        L"Provenienza:",
        L"Esterno",
        L"Descrizione\\s+Esame:",
        L"Quesito\\s+Diagnostico:",
        // Footer e note legali
        L"il:\\s+\\d{2}/\\d{2}/\\d{4}",
        L"Ora:\\s+\\d{2}:\\d{2}",
        L"Note\\s+di\\s+reperibilit",
        L"Le\\s+informazioni\\s+sanitarie",
        L"medico\\s+curante",
        L"Documento\\s+elettronico\\s+firmato",
        L"DPR\\s+445/2000",
        L"D\\.Lgs\\.\\s+82/2005",
        L"Tutti\\s+gli\\s+esami\\s+sono\\s+archiviati",
        L"mancata\\s+consegna\\s+del\\s+supporto",
        L"richiederne\\s+copia",
        L"Istituti\\s+Clinici\\s+Scientifici\\s+Spa",
        L"Pagina\\s+\\d+\\s+di\\s+\\d+",
        L"Sistema\\s+Sanitario",
        L"Regione\\s+Lombardia",
        // Testo dopo firma medico
        L"che,\\s+nel\\s+caso\\s+di\\s+dubbi",
        L"necessit.\\s+di\\s+approfondimenti",
        L"pu.\\s+rivolgersi\\s+allo\\s+specialista",
        L"che\\s+ha\\s+redatto\\s+il\\s+referto",
        L"il\\s+\\d{2}/\\d{2}/\\d{2}\\s+alle\\s+\\d{2}:\\d{2}",
    };

    p.keepPatterns = {
        L"Referto\\s+firmato\\s+digitalmente\\s+da:",
        L"CONCLUSIONI",
        L"FOLLOW\\s+UP",
    };

    p.newlineBeforePatterns = {
        L"DISTRETTO CAROTIDEO SIN",
        L"ARTERIE VERTEBRALI",
        L"ARTERIE SUCCLAVIE",
        L"CONCLUSIONI",
        L"FOLLOW UP",
        L"Referto firmato",
    };

    return p;
}

// ============================================================================
// PROFILO DEFAULT
// ============================================================================
ReportProfile ProfileManager::CreateDefaultProfile() {
    ReportProfile p;
    p.name = L"default";
    
    p.identifierPatterns = {};
    
    p.patientNamePatterns = {
        L"Sig\\./Sig\\.ra:\\s+([A-Za-z][A-Za-z\\s]+?)(?:\\s{2,}|ID)",
        L"Sig\\.\\s+([A-Za-z][A-Za-z\\s]+?)(?:\\s{2,}|ID)",
        L"Paziente:\\s+([A-Za-z][A-Za-z\\s]+?)(?:\\s{2,}|Data)",
    };
    
    p.excludePatterns = {
        L"Istituto",
        L"IRCCS",
        L"ASST",
        L"Ospedale",
        L"Servizio\\s+di",
        L"Primario:",
        L"Direttore:",
        L"Tel\\.",
        L"Fax\\.",
        L"Email:",
        L"Sig\\./Sig\\.ra:",
        L"Sig\\.\\s+[A-Z]",
        L"Data\\s+di\\s+Nascita:",
        L"Data\\s+nascita:",
        L"\\d{2}/\\d{2}/\\d{4}",
        L"Codice\\s+Fiscale:",
        L"[A-Z]{6}\\d{2}[A-Z]\\d{2}[A-Z]\\d{3}[A-Z]",
        L"ID\\s+Paziente:",
        L"N\\.\\s+di\\s+accesso:",
        L"\\d{10}",
        L"Provenienza:",
        L"Prestazione:",
        L"Schedulazione:",
        L"Esecuzione:",
        L"Data\\s+validazione",
        L"Documento\\s+informatico",
        L"stampa\\s+costituisce",
        L"D\\.Lgs",
        L"Pag\\s+\\d+\\s+di\\s+\\d+",
        L"TSRM:",
        L"Tecnico:",
    };
    
    p.keepPatterns = {
        L"Medico\\s+Radiologo:",
        L"Medico\\s+refertante:",
    };
    
    return p;
}
