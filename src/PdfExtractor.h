#pragma once
#include <string>
#include <vector>

// Struttura per definire una zona di estrazione
struct PdfZone {
    double x;       // Coordinata X (punti PDF)
    double y;       // Coordinata Y (punti PDF, origine in alto-sinistra per pdftotext)
    double width;   // Larghezza
    double height;  // Altezza
    int page;       // Pagina (1-indexed per pdftotext)
};

class PdfExtractor {
public:
    // Estrae il testo da un file PDF usando pdftotext.exe (intero documento)
    // Restituisce il testo estratto o una stringa vuota in caso di errore
    static std::wstring Extract(const std::wstring& pdfPath);

    // Estrae il testo da una zona specifica di una pagina
    // Le coordinate sono in punti PDF (72 punti = 1 pollice)
    // page e' 1-indexed (1 = prima pagina)
    static std::wstring ExtractZone(const std::wstring& pdfPath, const PdfZone& zone);

    // Estrae il testo da multiple zone
    static std::wstring ExtractZones(const std::wstring& pdfPath, const std::vector<PdfZone>& zones);

    // Ottiene il numero di pagine del PDF
    static int GetPageCount(const std::wstring& pdfPath);

    // Estrae testo usando PyMuPDF via script Python (piu' preciso per le zone)
    static std::wstring ExtractWithPython(const std::wstring& pdfPath, const std::wstring& profilePath);

    // Verifica se Python e PyMuPDF sono disponibili
    static bool IsPythonAvailable();

    // Verifica se pdftotext.exe è disponibile
    static bool IsAvailable();

    // Restituisce l'ultimo messaggio di errore
    static std::wstring GetLastError();

private:
    static std::wstring lastError;
    static std::wstring ExecutePdftotext(const std::wstring& pdfPath, const std::wstring& additionalArgs);
};
