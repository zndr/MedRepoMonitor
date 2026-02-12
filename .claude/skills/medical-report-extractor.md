---
name: medical-report-extractor
description: >
  Estrazione strutturata del corpo testuale da referti medici italiani in formato PDF.
  Usare SEMPRE quando l'utente carica referti medici di qualsiasi tipo (lettere di dimissione,
  referti radiologici, ecografie, esami strumentali, referti di laboratorio, anatomia patologica,
  referti cardiologici, endoscopici, manometrici, audiometrici, o qualsiasi altro documento clinico)
  e richiede l'estrazione del testo clinico rilevante. Attivare anche quando l'utente chiede di
  "leggere", "estrarre", "analizzare", "riassumere" un referto medico PDF, o quando carica un PDF
  che appare essere un documento clinico italiano. Gestisce sia PDF con testo selezionabile sia
  PDF da scansione o fotografia tramite OCR.
---

# Medical Report Extractor

Skill per l'estrazione strutturata e l'analisi del corpo testuale da referti medici italiani.

## Workflow di estrazione

### Step 1 — Rilevamento tipo PDF

Verificare se il PDF contiene testo selezionabile:

```python
import subprocess
result = subprocess.run(["pdftotext", pdf_path, "-"], capture_output=True, text=True)
has_text = len(result.stdout.strip()) > 50
```

- **PDF con testo**: procedere direttamente all'estrazione.
- **PDF grafico** (scansione o foto): attivare pipeline OCR (vedi sezione OCR).

### Step 2 — Estrazione testo grezzo

Estrarre il testo completo dal PDF mantenendo il layout per facilitare il riconoscimento delle sezioni.

### Step 3 — Pulizia e filtraggio

Applicare le regole di esclusione/inclusione (vedi sotto) per isolare il corpo del referto.

### Step 4 — Identificazione tipologia referto

Classificare il referto secondo le tipologie supportate (vedi sezione Tipologie). Se il referto non rientra in nessuna tipologia nota, applicare le regole generiche di estrazione.

### Step 5 — Estrazione reperti patologici

Analizzare il testo clinico e identificare i reperti significativi in senso patologico, classificandoli per severità.

### Step 6 — Composizione output

Assemblare l'output nel formato strutturato definito.

---

## Pipeline OCR per PDF grafici

Quando il PDF non contiene testo selezionabile (scansioni, fotografie da smartphone):

```python
# 1. Convertire pagine PDF in immagini ad alta risoluzione
#    Usare pdf2image / poppler con DPI >= 300

# 2. Pre-processing immagine (importante per foto smartphone)
#    - Correzione rotazione/angolazione (deskew)
#    - Miglioramento contrasto
#    - Binarizzazione adattiva
#    - Rimozione rumore

# 3. OCR con Tesseract
#    Lingue: ita+eng (molti referti contengono termini latini/inglesi)
#    PSM consigliato: 6 (blocco di testo uniforme) o 3 (automatico)

# 4. Post-processing OCR
#    - Correzione errori comuni (es. "rrn" → "mm", "I" → "l" in contesto)
#    - Ricostruzione parole spezzate
#    - Verifica coerenza con terminologia medica
```

Se la qualità OCR è insufficiente (testo illeggibile, confidenza bassa), informare l'utente e suggerire di caricare un'immagine migliore, specificando il problema riscontrato (bassa risoluzione, angolazione eccessiva, sfocatura).

---

## Regole di filtraggio

### Elementi da ESCLUDERE (sempre)

- Header istituzionale: logo, nome ospedale/struttura, indirizzo, recapiti, codici struttura
- Footer: note legali, firma digitale, numerazione pagine, informativa privacy, codici a barre
- Colonna laterale: équipe medica, numeri di telefono, email, orari segreteria, info CUP
- Dati anagrafici paziente: nome, cognome, data di nascita, codice fiscale, tessera sanitaria, ID paziente, indirizzo di residenza
- Metadati amministrativi: provenienza, numero ricovero/accesso, numero nosologico, codice impegnativa, medico prescrittore, reparto richiedente
- Quesito diagnostico/clinico (la domanda posta dal medico richiedente)
- Codici di classificazione: codici DRG, codici ICD-9/ICD-10, codici nomenclatore, codici prestazione

### Elementi da INCLUDERE (sempre)

- Corpo del referto: descrizione dell'esame, reperti, diagnosi, decorso clinico
- Conclusioni diagnostiche e raccomandazioni follow-up
- Terapia prescritta (con posologia dettagliata)
- Procedure eseguite e loro risultati

### Elementi da estrarre SEPARATAMENTE

- **Data di redazione del referto** (non la data dell'esame o del ricovero, ma la data di stesura/firma del documento)
- **Medico firmante** (nome, cognome, titolo — solo chi firma il referto)

---

## Estrazione reperti patologici significativi

Analizzare il testo del referto e identificare ogni reperto che si discosta dalla normalità o ha rilevanza clinica. Per ogni reperto:

### Classificazione di severità

- **(+++)** **Urgente/critico** — Richiede attenzione immediata o approfondimento urgente. Esempi: neoplasia sospetta, versamento pericardico significativo, embolia polmonare, stenosi critica, frattura instabile, pneumotorace.
- **(++)** **Da monitorare** — Anomalia che richiede follow-up o controllo nel tempo. Esempi: nodulo tiroideo < 1 cm, lieve ipertrofia ventricolare, diverticolosi, ernia discale senza deficit neurologico, cisti renale Bosniak II.
- **(+)** **Incidentale/minore** — Reperto anormale ma di scarsa rilevanza clinica immediata. Esempi: cisti epatiche semplici, calcificazioni aortiche minime, lipoma, osteofitosi degenerativa lieve.

### Formato reperto

Ogni reperto va espresso come:

```
(severità) Reperto sintetico — Dettaglio dal testo originale
```

Esempio:
```
(+++) Nodulo polmonare sospetto — "Formazione nodulare solida di 18 mm al lobo superiore destro, a margini spiculati, con enhancement contrastografico"
(++) Versamento pleurico lieve — "Modesta falda di versamento pleurico a destra"
(+) Cisti epatica semplice — "Formazione ipodensa di 12 mm al VII segmento, compatibile con cisti semplice"
```

---

## Formato output

L'output deve rispettare SEMPRE questa struttura, nell'ordine esatto indicato:

```
REPERTI PATOLOGICI SIGNIFICATIVI
[lista reperti con severità, ordinati: (+++) prima, poi (++), poi (+)]
[Se nessun reperto patologico: "Nessun reperto patologico significativo rilevato."]

________________________________________________________________________________

TESTO COMPLETO DEL REFERTO
[corpo del referto estratto, testo pulito]

________________________________________________________________________________

Data referto: [GG/MM/AAAA]
Medico: [Titolo Nome Cognome]
```

### Regole di formattazione del testo

- Testo continuo: unire le righe spezzate dal layout PDF
- A capo solo dopo segni di interpunzione che chiudono una frase (punto) o tra sezioni logiche
- Preservare la struttura logica originale del referto (sezioni, paragrafi, sottotitoli)
- Nessun markdown aggiuntivo nel corpo del referto: solo testo pulito
- Preservare i valori numerici esattamente come riportati (misure, dosaggi, valori lab)

### Eccezione: elenchi farmacologici

Quando il testo contiene elenchi di farmaci con posologia, preservare un farmaco per riga:

```
Terapia consigliata alla dimissione:
Madopar HBS disp 100+25 mg 1+1 cp ore 8-20
Clopidogrel 75 mg 1 cp ore 13
Bisoprololo 1.25 mg 1 cp ore 8
```

---

## Tipologie di referto supportate

### Referto radiologico (RX, TC, RM)
Estrarre: tipo di esame, tecnica/protocollo, mezzo di contrasto se usato, descrizione dei reperti per distretto, conclusioni diagnostiche, raccomandazioni follow-up.

### Referto ecografico (ecografia, ecocolordoppler)
Estrarre: tipo di esame, distretto esaminato, reperti per struttura anatomica, misurazioni, conclusioni, indicazioni follow-up.

### Referto endoscopico (EGDS, colonscopia, broncoscopia)
Estrarre: tipo di esame, sedazione, descrizione per segmento anatomico, biopsie eseguite, conclusioni, indicazioni terapeutiche.

### Referto di anatomia patologica / istologico
Estrarre: materiale esaminato, descrizione macroscopica, descrizione microscopica, diagnosi istologica, classificazione (TNM, grading se presente), note immunoistochimiche.

### Referto cardiologico (ECG, ecocardiogramma, Holter, test da sforzo)
Estrarre: tipo di esame, parametri misurati, reperti significativi, conclusioni, raccomandazioni.

### Referto di laboratorio analisi
Estrarre: elenco esami con valore, unità di misura e range di riferimento. Segnalare come reperti patologici i valori fuori range.

### Referto funzionale (spirometria, manometria, EMG, EEG, audiometria)
Estrarre: tipo di esame, parametri misurati, valori e riferimenti, interpretazione, conclusioni.

### Lettera di dimissione ospedaliera
Vedi sezione dedicata sotto.

### Tipologia non riconosciuta
Se il referto non rientra nelle tipologie sopra, applicare le regole generiche: estrarre tutto il testo che descrive reperti clinici, diagnosi, procedure e terapie. Escludere header/footer/dati amministrativi come di consueto.

---

## Lettera di dimissione ospedaliera

Le lettere di dimissione sono documenti complessi, spesso multi-pagina, con molte sezioni. Richiedono un trattamento specifico.

### Sezioni da estrarre (nell'ordine in cui compaiono)

Estrarre e preservare la struttura in sezioni del documento originale. Le sezioni tipiche sono (non tutte sempre presenti):

1. **Diagnosi alla dimissione** — Diagnosi principale e secondarie
2. **Motivo del ricovero** — Sintomo/evento che ha portato al ricovero
3. **Anamnesi patologica rilevante** — Solo se inclusa nel corpo della lettera (non i dati anagrafici)
4. **Decorso clinico** — Descrizione di quanto avvenuto durante il ricovero
5. **Esami diagnostici eseguiti** — Risultati di laboratorio, imaging, esami strumentali
6. **Procedure/interventi** — Descrizione delle procedure eseguite con data
7. **Consulenze specialistiche** — Sintesi delle consulenze richieste
8. **Condizioni alla dimissione** — Stato clinico del paziente alla dimissione
9. **Terapia alla dimissione** — Elenco farmaci con posologia (un farmaco per riga)
10. **Indicazioni al follow-up** — Controlli programmati, esami da eseguire, quando tornare
11. **Indicazioni dietetiche/comportamentali** — Se presenti

### Reperti patologici nelle lettere di dimissione

Per le lettere di dimissione, i reperti patologici da estrarre includono:
- Le diagnosi alla dimissione (con severità appropriata)
- Risultati anomali degli esami eseguiti durante il ricovero
- Complicanze insorte durante il decorso
- Qualsiasi condizione che richiede follow-up attivo

Non includere come reperto patologico le condizioni croniche note elencate nell'anamnesi, a meno che non siano oggetto specifico del ricovero.

---

## Apprendimento dalle correzioni dell'utente

Quando l'utente corregge l'output (elimina parti, segnala errori, richiede modifiche), questo rappresenta un'opportunità per migliorare la skill.

### Come apprendere

1. **Identificare il pattern**: Quando l'utente elimina una porzione di testo o segnala che non andava estratta, identificare *quale regola* avrebbe dovuto escluderla. Esempio: l'utente elimina ripetutamente la sezione "Quesito clinico" → la regola di esclusione per il quesito diagnostico va resa più aggressiva o specifica.

2. **Formulare la regola**: Esprimere la correzione come regola generalizzabile, non specifica al singolo documento. Esempio corretto: "Escludere la sezione 'Indicazione clinica all'esame' e sue varianti" — Esempio errato: "Nel referto del 15/01/2026 togliere la terza riga".

3. **Proporre l'aggiornamento**: Presentare all'utente la nuova regola dedotta e chiedere conferma prima di modificare la skill.

4. **Aggiornare la skill**: Aggiungere la regola nella sezione appropriata di questo SKILL.md (esclusione, inclusione, formattazione, o nella tipologia specifica).

### Dove registrare le regole apprese

Aggiungere le regole apprese nella sezione sottostante, con data e breve descrizione dell'origine:

```
## Regole apprese dalle correzioni

[Questa sezione si popola nel tempo con le regole dedotte dalle correzioni dell'utente]

<!-- Formato:
- [AAAA-MM-GG] ESCLUDERE/INCLUDERE/FORMATTARE: descrizione regola — Origine: cosa ha corretto l'utente
-->
```

---

## Regole apprese dalle correzioni

<!-- Sezione auto-aggiornata. Ogni regola è derivata da una correzione dell'utente confermata. -->
<!-- Formato: - [AAAA-MM-GG] AZIONE: regola — Origine: descrizione correzione -->

---

## Note tecniche

### Dipendenze per OCR
- `poppler-utils` (pdftotext, pdfinfo)
- `tesseract-ocr` con language pack `tesseract-ocr-ita`
- `pdf2image` (Python, per conversione pagine in immagini)
- `Pillow` (Python, per pre-processing immagini)

### Gestione PDF multi-pagina
- Processare tutte le pagine in sequenza
- Applicare le regole di esclusione header/footer a OGNI pagina
- Ricostruire il testo continuo eliminando le interruzioni di pagina artificiali
- Per le lettere di dimissione, riconoscere che le sezioni possono attraversare i confini di pagina
