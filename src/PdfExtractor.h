#pragma once
#include <string>

class PdfExtractor {
public:
    // Estrae il testo da un file PDF usando pdftotext.exe
    // Restituisce il testo estratto o una stringa vuota in caso di errore
    static std::wstring Extract(const std::wstring& pdfPath);
    
    // Verifica se pdftotext.exe è disponibile
    static bool IsAvailable();
    
    // Restituisce l'ultimo messaggio di errore
    static std::wstring GetLastError();
    
private:
    static std::wstring lastError;
};
