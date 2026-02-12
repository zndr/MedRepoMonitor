import fitz  # PyMuPDF
import json
import os
from tkinter import Tk, filedialog

class PDFTextExtractor:
    def __init__(self, pdf_path, profile_path):
        self.pdf_path = pdf_path
        self.doc = fitz.open(pdf_path)
        
        # Carica profilo
        with open(profile_path, 'r', encoding='utf-8') as f:
            self.profile = json.load(f)
        
        print(f"\n{'='*60}")
        print(f"PDF: {os.path.basename(pdf_path)}")
        print(f"Profilo: {self.profile['profile_name']}")
        print(f"Pagine totali: {len(self.doc)}")
        print(f"Zone definite: {len(self.profile['zones'])}")
        print(f"{'='*60}\n")
    
    def _zone_applies_to_page(self, zone, page_num):
        """Verifica se una zona si applica alla pagina specificata"""
        pages = zone.get('pages', 'current')
        if pages == 'all':
            return True
        elif isinstance(pages, list):
            return page_num in pages
        elif isinstance(pages, int):
            return page_num == pages
        return False
    
    def _format_pages_info(self, pages):
        """Formatta info pagine"""
        if pages == 'all':
            return "tutte le pagine"
        elif isinstance(pages, list):
            if len(pages) == 1:
                return f"pagina {pages[0] + 1}"
            else:
                page_nums = [str(p + 1) for p in pages]
                return f"pagine {', '.join(page_nums)}"
        elif isinstance(pages, int):
            return f"pagina {pages + 1}"
        return ""
    
    def extract_text_from_zone(self, page, zone):
        """Estrae testo da una zona specifica di una pagina"""
        # Crea rettangolo con coordinate PDF
        # PyMuPDF usa: (x0, y0, x1, y1) dove (x0,y0) è in basso a sinistra
        rect = fitz.Rect(
            zone['x'],
            zone['y'],
            zone['x'] + zone['width'],
            zone['y'] + zone['height']
        )
        
        # Estrai testo dalla zona
        text = page.get_text("text", clip=rect)
        
        # Pulisci testo (rimuovi spazi multipli, newline eccessive)
        text = text.strip()
        lines = [line.strip() for line in text.split('\n') if line.strip()]
        text = '\n'.join(lines)
        
        return text
    
    def extract_all_zones(self, output_format='text'):
        """
        Estrae testo da tutte le zone definite nel profilo
        
        Args:
            output_format: 'text' (leggibile), 'json' (strutturato), 'dict' (oggetto Python)
        
        Returns:
            str o dict con il testo estratto
        """
        results = {}
        
        # Processa ogni pagina
        for page_num in range(len(self.doc)):
            page = self.doc[page_num]
            
            # Trova zone applicabili a questa pagina
            for zone in self.profile['zones']:
                if self._zone_applies_to_page(zone, page_num):
                    zone_label = zone['label']
                    
                    # Inizializza lista per questa zona se non esiste
                    if zone_label not in results:
                        results[zone_label] = {
                            'pages': [],
                            'texts': []
                        }
                    
                    # Estrai testo
                    text = self.extract_text_from_zone(page, zone)
                    
                    if text:  # Solo se c'è testo
                        results[zone_label]['pages'].append(page_num + 1)
                        results[zone_label]['texts'].append(text)
        
        # Formatta output
        if output_format == 'dict':
            return results
        elif output_format == 'json':
            return json.dumps(results, indent=2, ensure_ascii=False)
        else:  # text
            return self._format_text_output(results)
    
    def _format_text_output(self, results):
        """Formatta risultati in testo leggibile"""
        output = []
        
        for zone_label, data in results.items():
            output.append(f"\n{'='*60}")
            output.append(f"ZONA: {zone_label}")
            output.append(f"{'='*60}")
            
            if not data['texts']:
                output.append("(nessun testo estratto)")
            else:
                # Se testo da più pagine, mostra separatamente
                if len(data['texts']) > 1:
                    for page, text in zip(data['pages'], data['texts']):
                        output.append(f"\n--- Pagina {page} ---")
                        output.append(text)
                else:
                    # Singola pagina o testo combinato
                    output.append(f"Pagina: {data['pages'][0]}")
                    output.append(f"\n{data['texts'][0]}")
        
        return '\n'.join(output)
    
    def extract_zone_by_name(self, zone_name):
        """Estrae testo da una zona specifica per nome"""
        zone = next((z for z in self.profile['zones'] if z['label'] == zone_name), None)
        
        if not zone:
            print(f"✗ Zona '{zone_name}' non trovata nel profilo")
            return None
        
        results = []
        pages_info = self._format_pages_info(zone['pages'])
        
        print(f"\nEstrazione zona '{zone_name}' da {pages_info}...")
        
        for page_num in range(len(self.doc)):
            if self._zone_applies_to_page(zone, page_num):
                page = self.doc[page_num]
                text = self.extract_text_from_zone(page, zone)
                
                if text:
                    results.append({
                        'page': page_num + 1,
                        'text': text
                    })
        
        return results
    
    def preview_zones(self):
        """Mostra anteprima di tutte le zone con primi 100 caratteri"""
        print(f"\n{'='*60}")
        print("ANTEPRIMA ESTRAZIONE")
        print(f"{'='*60}\n")
        
        for zone in self.profile['zones']:
            zone_label = zone['label']
            pages_info = self._format_pages_info(zone['pages'])
            
            print(f"Zona: {zone_label} ({pages_info})")
            
            # Estrai da prima pagina applicabile
            first_text = None
            for page_num in range(len(self.doc)):
                if self._zone_applies_to_page(zone, page_num):
                    page = self.doc[page_num]
                    text = self.extract_text_from_zone(page, zone)
                    if text:
                        first_text = text
                        break
            
            if first_text:
                preview = first_text[:100].replace('\n', ' ')
                if len(first_text) > 100:
                    preview += "..."
                print(f"  → {preview}")
            else:
                print(f"  → (nessun testo)")
            print()
    
    def save_to_file(self, output_path, format='text'):
        """Salva risultati in file"""
        content = self.extract_all_zones(output_format=format)
        
        with open(output_path, 'w', encoding='utf-8') as f:
            f.write(content)
        
        print(f"\n✓ Risultati salvati in: {output_path}")
    
    def close(self):
        """Chiude il documento PDF"""
        self.doc.close()


def select_file(title, filetypes):
    """Helper per selezione file"""
    root = Tk()
    root.withdraw()
    root.attributes('-topmost', True)
    
    file_path = filedialog.askopenfilename(
        title=title,
        filetypes=filetypes,
        initialdir=os.getcwd()
    )
    
    root.destroy()
    return file_path


def main():
    print("\n" + "="*60)
    print("PDF TEXT EXTRACTOR - Estrazione con profili")
    print("="*60 + "\n")
    
    # Seleziona PDF
    pdf_path = select_file(
        "Seleziona il PDF da cui estrarre il testo",
        [("PDF files", "*.pdf"), ("All files", "*.*")]
    )
    
    if not pdf_path:
        print("✗ Nessun PDF selezionato. Uscita.")
        return
    
    # Seleziona profilo JSON
    profile_path = select_file(
        "Seleziona il profilo JSON",
        [("JSON files", "*.json"), ("All files", "*.*")]
    )
    
    if not profile_path:
        print("✗ Nessun profilo selezionato. Uscita.")
        return
    
    # Crea estrattore
    try:
        extractor = PDFTextExtractor(pdf_path, profile_path)
    except Exception as e:
        print(f"✗ Errore apertura file: {e}")
        return
    
    # Menu interattivo
    while True:
        print("\n" + "="*60)
        print("OPZIONI:")
        print("="*60)
        print("1. Anteprima zone (primi 100 caratteri)")
        print("2. Estrai tutto e mostra a schermo")
        print("3. Estrai tutto e salva in file TXT")
        print("4. Estrai tutto e salva in JSON")
        print("5. Estrai zona specifica per nome")
        print("6. Esci")
        print("="*60)
        
        choice = input("\nScelta: ").strip()
        
        if choice == '1':
            extractor.preview_zones()
        
        elif choice == '2':
            print(extractor.extract_all_zones(output_format='text'))
        
        elif choice == '3':
            output_path = f"extracted_{os.path.splitext(os.path.basename(pdf_path))[0]}.txt"
            extractor.save_to_file(output_path, format='text')
        
        elif choice == '4':
            output_path = f"extracted_{os.path.splitext(os.path.basename(pdf_path))[0]}.json"
            extractor.save_to_file(output_path, format='json')
        
        elif choice == '5':
            # Mostra zone disponibili
            print("\nZone disponibili:")
            for i, zone in enumerate(extractor.profile['zones'], 1):
                print(f"  {i}. {zone['label']}")
            
            zone_name = input("\nNome zona da estrarre: ").strip()
            results = extractor.extract_zone_by_name(zone_name)
            
            if results:
                print(f"\n{'='*60}")
                print(f"ZONA: {zone_name}")
                print(f"{'='*60}")
                for item in results:
                    print(f"\n--- Pagina {item['page']} ---")
                    print(item['text'])
        
        elif choice == '6':
            break
        
        else:
            print("⚠ Scelta non valida")
    
    extractor.close()
    print("\n✓ Chiuso. Arrivederci!\n")


if __name__ == "__main__":
    main()