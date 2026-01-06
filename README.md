# Medical Report Monitor

Applicazione Windows per il monitoraggio di una directory e l'estrazione automatica del testo da referti medici in formato PDF.

## Funzionalità

- Monitoraggio continuo di una directory per nuovi file PDF
- Estrazione del testo dal PDF con pdftotext
- Parsing intelligente secondo le regole della skill medical-report-extractor
- Copia automatica del testo estratto nella clipboard di Windows
- Salvataggio di un file .txt di backup con il nome del paziente
- Notifica visiva al completamento dell'estrazione
- Avvio automatico all'avvio di Windows (opzionale)

## Requisiti

- Windows 10/11 (64-bit)
- Visual Studio 2022 con supporto C++17
- **pdftotext.exe** da Xpdf Tools

## Installazione pdftotext

1. Scarica Xpdf command line tools da: https://www.xpdfreader.com/download.html
2. Estrai l'archivio
3. Copia `pdftotext.exe` nella stessa cartella dell'eseguibile MedicalReportMonitor.exe

## Compilazione

1. Apri `MedicalReportMonitor.sln` con Visual Studio 2022
2. Seleziona la configurazione `Release | x64`
3. Compila con `Ctrl+Shift+B` o menu Build > Build Solution
4. L'eseguibile sarà in `bin\Release\MedicalReportMonitor.exe`

## Configurazione

Al primo avvio, il programma chiederà:

1. **Directory da monitorare**: la cartella dove verranno salvati i PDF dei referti
2. **Directory output**: dove salvare i file .txt estratti (opzionale, default = stessa cartella dei PDF)
3. **Autostart**: se avviare automaticamente il programma all'avvio di Windows

La configurazione viene salvata in `config.ini` nella stessa cartella dell'eseguibile.

## Utilizzo

1. Avvia `MedicalReportMonitor.exe`
2. Il programma inizierà a monitorare la directory configurata
3. Quando viene rilevato un nuovo PDF:
   - Il testo viene estratto e analizzato
   - Il corpo del referto viene copiato nella clipboard
   - Viene salvato un file .txt con il nome del paziente
   - Appare una notifica di conferma
4. Premi `Q` per chiudere il programma

## Regole di estrazione

L'applicazione applica automaticamente le seguenti regole:

### Elementi rimossi
- Header istituzionali (logo, nome ospedale, indirizzi)
- Footer (note legali, firma digitale, numerazione pagine)
- Colonna laterale sinistra (équipe medica, contatti, CUP)
- Dati anagrafici paziente (eccetto il nome per il filename)
- Metadati amministrativi (provenienza, quesito diagnostico, date)

### Elementi mantenuti
- Corpo del referto (diagnosi, descrizione, reperti)
- Sezione terapia farmacologica (con formattazione preservata)
- Conclusioni e follow-up
- Firma del medico refertante

## Struttura del progetto

```
MedicalReportMonitor/
├── MedicalReportMonitor.sln
├── MedicalReportMonitor.vcxproj
├── README.md
└── src/
    ├── main.cpp              # Entry point e logica principale
    ├── Config.h/cpp          # Gestione configurazione e autostart
    ├── FileWatcher.h/cpp     # Monitoraggio directory
    ├── PdfExtractor.h/cpp    # Estrazione testo da PDF
    ├── TextParser.h/cpp      # Parsing secondo regole skill
    └── ClipboardHelper.h/cpp # Gestione clipboard Windows
```

## Note

- L'estrazione del testo è basata su pattern matching e regex, quindi potrebbe non essere perfetta per tutti i formati di referto
- Per referti con layout molto diversi da quelli standard, potrebbe essere necessario modificare i pattern in `TextParser.cpp`
- Il programma supporta solo PDF di testo, non PDF con immagini scansionate (per questi servirebbe OCR)
