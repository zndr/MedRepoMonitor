#include "ClipboardHelper.h"

std::wstring ClipboardHelper::lastError;

bool ClipboardHelper::CopyToClipboard(const std::wstring& text) {
    lastError.clear();
    
    if (!OpenClipboard(NULL)) {
        lastError = L"Impossibile aprire la clipboard";
        return false;
    }
    
    if (!EmptyClipboard()) {
        CloseClipboard();
        lastError = L"Impossibile svuotare la clipboard";
        return false;
    }
    
    // Alloca memoria globale per il testo
    size_t size = (text.length() + 1) * sizeof(wchar_t);
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, size);
    
    if (!hMem) {
        CloseClipboard();
        lastError = L"Impossibile allocare memoria";
        return false;
    }
    
    // Copia il testo nella memoria allocata
    wchar_t* pMem = static_cast<wchar_t*>(GlobalLock(hMem));
    if (!pMem) {
        GlobalFree(hMem);
        CloseClipboard();
        lastError = L"Impossibile bloccare la memoria";
        return false;
    }
    
    wcscpy_s(pMem, text.length() + 1, text.c_str());
    GlobalUnlock(hMem);
    
    // Imposta i dati nella clipboard
    if (!SetClipboardData(CF_UNICODETEXT, hMem)) {
        GlobalFree(hMem);
        CloseClipboard();
        lastError = L"Impossibile impostare i dati nella clipboard";
        return false;
    }
    
    CloseClipboard();
    return true;
}

std::wstring ClipboardHelper::GetLastError() {
    return lastError;
}
