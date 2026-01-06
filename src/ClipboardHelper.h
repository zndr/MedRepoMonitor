#pragma once
#include <string>
#include <Windows.h>

class ClipboardHelper {
public:
    // Copia il testo nella clipboard di Windows
    static bool CopyToClipboard(const std::wstring& text);
    
    // Restituisce l'ultimo errore
    static std::wstring GetLastError();
    
private:
    static std::wstring lastError;
};
