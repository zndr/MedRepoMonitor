#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Script minimale per estrazione testo da zone PDF usando PyMuPDF.
Chiamato dal programma C++ MedicalReportMonitor.

Uso: python extract_zones.py <pdf_path> <profile_json_path>
Output: testo estratto su stdout (UTF-8)
"""

import sys
import json
import re
import fitz  # PyMuPDF


# Parole singole che vanno trattate come intestazioni speciali
SPECIAL_HEADERS = {'TERAPIA'}

# Intestazioni che richiedono di mantenere le righe separate (elenchi)
LIST_SECTION_HEADERS = {'TERAPIA'}

# Pattern che identificano un farmaco (dosaggi, unità di misura, forme farmaceutiche)
DRUG_PATTERNS = re.compile(
    r'\d+\s*(mg|ml|mg/ml|mcg|g|ui|cp|cpr|cprrv|cps|cpsrp|fl|gtt|bust|conf|supp|os|ev|im|sc|die|bid|tid|qid|prn|h\d+|ore|%)|'
    r'\b(compresse|capsule|fiale|gocce|bustine|supposte|cerotto|cerotti|sciroppo|crema|pomata|gel|spray)\b',
    re.IGNORECASE
)


def looks_like_drug(line):
    """
    Verifica se una riga sembra essere un farmaco (contiene dosaggi, unità, ecc.)
    """
    return bool(DRUG_PATTERNS.search(line))


def is_header_words(words):
    """
    Verifica se una lista di parole forma un'intestazione (tutte maiuscole).
    """
    if len(words) < 2:
        return False
    for word in words:
        # Rimuovi punteggiatura dalla parola
        clean_word = re.sub(r'[^\w]', '', word)
        if not clean_word:
            continue
        if not clean_word.isupper() or not any(c.isalpha() for c in clean_word):
            return False
    return True


def is_special_header(line):
    """
    Verifica se la riga e' un'intestazione speciale (parola singola maiuscola).
    """
    clean = re.sub(r'[^\w\s]', '', line).strip().upper()
    return clean in SPECIAL_HEADERS


def is_list_section_header(line):
    """
    Verifica se la riga e' un'intestazione che introduce un elenco (es. TERAPIA).
    """
    clean = re.sub(r'[^\w\s]', '', line).strip().upper()
    return clean in LIST_SECTION_HEADERS


def is_header_line(line):
    """
    Verifica se un'intera riga e' un'intestazione.
    - 2+ parole tutte in maiuscolo, OPPURE
    - Parola singola speciale (TERAPIA, DIAGNOSI, ecc.)
    """
    clean = re.sub(r'[^\w\s]', '', line).strip()
    words = clean.split()

    # Controlla se e' un'intestazione speciale (parola singola)
    if len(words) == 1 and clean.upper() in SPECIAL_HEADERS:
        return True

    return is_header_words(words)


def split_header_from_text(line, in_list_section=False):
    """
    Se una riga inizia con un'intestazione (2+ parole MAIUSCOLE) seguita da testo normale,
    separa l'intestazione dal resto.
    Ritorna una lista di righe (1 o 2 elementi).

    Esempio: "DISTRETTO CAROTIDEO DX Arteria carotide"
             -> ["DISTRETTO CAROTIDEO DX", "Arteria carotide"]

    Se in_list_section=True e la riga sembra un farmaco, NON fare lo split.
    """
    # Se siamo in una sezione lista e la riga sembra un farmaco, non splittare
    if in_list_section and looks_like_drug(line):
        return [line]

    words = line.split()
    if len(words) < 2:
        return [line]

    # Trova dove finiscono le parole maiuscole consecutive (minimo 2)
    header_end = 0
    for i, word in enumerate(words):
        clean_word = re.sub(r'[^\w]', '', word)
        if clean_word and clean_word.isupper() and any(c.isalpha() for c in clean_word):
            header_end = i + 1
        else:
            break

    # Se abbiamo almeno 2 parole maiuscole all'inizio e c'e' altro testo dopo
    if header_end >= 2 and header_end < len(words):
        # Controlla se il resto contiene pattern farmaco - in tal caso non splittare
        rest = ' '.join(words[header_end:])
        if looks_like_drug(rest) or looks_like_drug(line):
            return [line]

        header = ' '.join(words[:header_end])
        return [header, rest]

    return [line]


def preprocess_lines(text):
    """
    Preprocessa il testo separando le intestazioni dal testo che le segue sulla stessa riga.
    NON separa se la riga contiene pattern farmaco.
    """
    lines = text.split('\n')
    result = []

    for line in lines:
        line = line.strip()
        if not line:
            continue

        # Se la riga sembra un farmaco, non fare lo split
        if looks_like_drug(line):
            result.append(line)
        else:
            # Separa eventuale intestazione dal testo
            split_lines = split_header_from_text(line)
            result.extend(split_lines)

    return result


def format_text(text):
    """
    Formatta il testo unendo le righe superflue.
    Criteri:
    - Prima separa le intestazioni dal testo sulla stessa riga
    - Le righe tra due intestazioni (2+ parole MAIUSCOLE) vengono unite
    - Le righe che iniziano con '-' restano separate
    - Le intestazioni restano su riga propria
    - Dopo TERAPIA (e simili), le righe restano separate fino alla prossima intestazione
    - Nella sezione TERAPIA, i farmaci (riconosciuti da pattern) non interrompono la lista
    """
    # Prima preprocessa: separa intestazioni dal testo sulla stessa riga
    lines = preprocess_lines(text)

    if not lines:
        return text

    result = []
    current_paragraph = []
    in_list_section = False  # True quando siamo dopo TERAPIA o simili

    for line in lines:
        line = line.strip()
        if not line:
            continue

        # Verifica se e' un'intestazione
        is_header = is_header_line(line)

        # Se siamo in una sezione lista, verifica se la riga e' un farmaco
        # In tal caso, NON trattarla come intestazione anche se ha 2+ maiuscole
        if in_list_section and is_header and looks_like_drug(line):
            is_header = False

        # Se e' un'intestazione
        if is_header:
            # Chiudi il paragrafo precedente
            if current_paragraph:
                result.append(' '.join(current_paragraph))
                current_paragraph = []

            result.append(line)

            # Controlla se entriamo in una sezione "lista" (es. TERAPIA)
            in_list_section = is_list_section_header(line)

        # Se inizia con trattino, mantieni separato
        elif line.startswith('-'):
            if current_paragraph:
                result.append(' '.join(current_paragraph))
                current_paragraph = []
            result.append(line)

        # Se siamo in una sezione lista (es. dopo TERAPIA), mantieni ogni riga separata
        elif in_list_section:
            if current_paragraph:
                result.append(' '.join(current_paragraph))
                current_paragraph = []
            result.append(line)

        # Altrimenti, aggiungi al paragrafo corrente
        else:
            current_paragraph.append(line)

    # Chiudi l'ultimo paragrafo
    if current_paragraph:
        result.append(' '.join(current_paragraph))

    return '\n'.join(result)


def extract_text_from_zone(page, zone):
    """Estrae testo da una zona specifica di una pagina"""
    rect = fitz.Rect(
        zone['x'],
        zone['y'],
        zone['x'] + zone['width'],
        zone['y'] + zone['height']
    )

    text = page.get_text("text", clip=rect)

    # Normalizza newline (rimuovi \r) e pulisci
    text = text.replace('\r\n', '\n').replace('\r', '\n')
    text = text.strip()
    lines = [line.strip() for line in text.split('\n') if line.strip()]
    return '\n'.join(lines)


def zone_applies_to_page(zone, page_num):
    """Verifica se una zona si applica alla pagina specificata (0-indexed)"""
    pages = zone.get('pages', 'current')
    if pages == 'all':
        return True
    elif isinstance(pages, list):
        return page_num in pages
    elif isinstance(pages, int):
        return page_num == pages
    return False


def main():
    if len(sys.argv) < 3:
        print("Uso: python extract_zones.py <pdf_path> <profile_json_path>", file=sys.stderr)
        sys.exit(1)

    pdf_path = sys.argv[1]
    profile_path = sys.argv[2]

    # Carica profilo
    try:
        with open(profile_path, 'r', encoding='utf-8') as f:
            profile = json.load(f)
    except Exception as e:
        print(f"Errore caricamento profilo: {e}", file=sys.stderr)
        sys.exit(2)

    # Apri PDF
    try:
        doc = fitz.open(pdf_path)
    except Exception as e:
        print(f"Errore apertura PDF: {e}", file=sys.stderr)
        sys.exit(3)

    # Estrai testo da tutte le zone
    all_texts = []

    for page_num in range(len(doc)):
        page = doc[page_num]

        for zone in profile.get('zones', []):
            if zone_applies_to_page(zone, page_num):
                text = extract_text_from_zone(page, zone)
                if text:
                    all_texts.append(text)

    doc.close()

    # Unisci testi e applica formattazione
    result = '\n'.join(all_texts)
    result = format_text(result)

    # Stampa in UTF-8
    sys.stdout.reconfigure(encoding='utf-8')
    print(result)


if __name__ == "__main__":
    main()
