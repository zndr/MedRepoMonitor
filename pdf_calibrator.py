import fitz
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle
from matplotlib.widgets import Button, TextBox, CheckButtons
import json
import os
from tkinter import Tk, filedialog

class PDFZoneCalibrator:
    def __init__(self, pdf_path, profile_name="default"):
        self.pdf_path = pdf_path
        self.profile_name = profile_name
        self.doc = fitz.open(pdf_path)
        self.current_page = 0
        self.zones = []
        self.zone_label = "zona_1"  # Nome default incrementale
        self.zone_pages_mode = "current"
        self.unsaved_changes = False
        self.auto_save = False
        self.zone_counter = 1  # Contatore per auto-naming
        
        self._load_page()
        self._setup_ui()
        self._update_info()
        self._update_title()
    
    def _load_page(self):
        """Carica e renderizza la pagina corrente"""
        self.page = self.doc[self.current_page]
        self.pix = self.page.get_pixmap(dpi=150)
        self.page_width = self.page.rect.width
        self.page_height = self.page.rect.height
        
        # Stato disegno
        self.drawing = False
        self.rect_start = None
        self.current_rect = None
    
    def _setup_ui(self):
        """Configura interfaccia grafica"""
        self.fig, self.ax = plt.subplots(figsize=(13, 15))
        plt.subplots_adjust(bottom=0.22, left=0.05, right=0.95)
        
        self._render_page()
        
        # Box nome zona con suggerimento
        ax_label = plt.axes([0.12, 0.13, 0.25, 0.03])
        self.label_box = TextBox(ax_label, 'Nome zona:', initial=self.zone_label)
        self.label_box.on_submit(self.update_label)
        
        # Radio button per modalità pagine
        ax_mode = plt.axes([0.12, 0.08, 0.25, 0.04])
        from matplotlib.widgets import RadioButtons
        self.radio_pages = RadioButtons(ax_mode, 
            ('Solo questa pagina', 'Tutte le pagine', 'Range personalizzato'),
            active=0)
        self.radio_pages.on_clicked(self.update_pages_mode)
        
        # Input range pagine
        ax_range = plt.axes([0.12, 0.04, 0.25, 0.03])
        self.range_box = TextBox(ax_range, 'Range (es: 2-4):', initial='')
        
        # Checkbox auto-save
        ax_check = plt.axes([0.12, 0.01, 0.25, 0.02])
        self.check_autosave = CheckButtons(ax_check, ['Salvataggio automatico'], [False])
        self.check_autosave.on_clicked(self.toggle_autosave)
        
        # Bottoni navigazione
        ax_prev = plt.axes([0.40, 0.13, 0.08, 0.03])
        self.btn_prev = Button(ax_prev, '◄ Pag. prec.')
        self.btn_prev.on_clicked(self.prev_page)
        
        ax_next = plt.axes([0.49, 0.13, 0.08, 0.03])
        self.btn_next = Button(ax_next, 'Pag. succ. ►')
        self.btn_next.on_clicked(self.next_page)
        
        # Bottoni azioni
        ax_save = plt.axes([0.40, 0.08, 0.08, 0.03])
        self.btn_save = Button(ax_save, 'Salva JSON')
        self.btn_save.on_clicked(self.save_profile)
        
        ax_clear = plt.axes([0.49, 0.08, 0.08, 0.03])
        self.btn_clear = Button(ax_clear, 'Cancella ultima')
        self.btn_clear.on_clicked(self.clear_last_zone)
        
        ax_load = plt.axes([0.40, 0.04, 0.08, 0.03])
        self.btn_load = Button(ax_load, 'Carica JSON')
        self.btn_load.on_clicked(self.load_profile)
        
        ax_show_all = plt.axes([0.49, 0.04, 0.08, 0.03])
        self.btn_show_all = Button(ax_show_all, 'Mostra tutte')
        self.btn_show_all.on_clicked(self.show_all_zones)
        
        ax_clear_all = plt.axes([0.40, 0.01, 0.08, 0.02])
        self.btn_clear_all = Button(ax_clear_all, 'Cancella tutte', color='#ffcccc')
        self.btn_clear_all.on_clicked(self.clear_all_zones)
        
        # Info text
        self.info_text = self.ax.text(0.02, 0.98, '', transform=self.ax.transAxes,
                                     verticalalignment='top', 
                                     bbox=dict(boxstyle='round', facecolor='yellow', alpha=0.8),
                                     fontsize=9, family='monospace')
        
        # Zone list text (nuovo)
        self.zones_list_text = self.ax.text(0.60, 0.98, '', transform=self.ax.transAxes,
                                            verticalalignment='top',
                                            bbox=dict(boxstyle='round', facecolor='lightblue', alpha=0.8),
                                            fontsize=8, family='monospace')
        
        # Eventi mouse
        self.fig.canvas.mpl_connect('button_press_event', self.on_press)
        self.fig.canvas.mpl_connect('motion_notify_event', self.on_motion)
        self.fig.canvas.mpl_connect('button_release_event', self.on_release)
        self.fig.canvas.mpl_connect('close_event', self.on_close)
    
    def _update_title(self):
        """Aggiorna titolo finestra con indicatore modifiche"""
        base_title = f"PDF Zone Calibrator - {os.path.basename(self.pdf_path)}"
        if self.unsaved_changes:
            self.fig.canvas.manager.set_window_title(f"{base_title} ● MODIFICHE NON SALVATE")
        else:
            self.fig.canvas.manager.set_window_title(base_title)
    
    def _mark_unsaved(self):
        """Marca come modificato e aggiorna UI"""
        self.unsaved_changes = True
        self._update_title()
        
        # Cambia colore bottone Salva
        self.btn_save.color = '#ffcccc'
        self.btn_save.hovercolor = '#ff9999'
        self.fig.canvas.draw_idle()
        
        # Auto-save se abilitato
        if self.auto_save:
            self.save_profile(None, silent=True)
    
    def _mark_saved(self):
        """Marca come salvato e aggiorna UI"""
        self.unsaved_changes = False
        self._update_title()
        
        # Ripristina colore bottone
        self.btn_save.color = '0.85'
        self.btn_save.hovercolor = '0.95'
        self.fig.canvas.draw_idle()
    
    def toggle_autosave(self, label):
        self.auto_save = not self.auto_save
        status = "ATTIVO" if self.auto_save else "DISATTIVATO"
        print(f"\n⚙ Salvataggio automatico: {status}")
    
    def _render_page(self):
        """Renderizza la pagina corrente"""
        self.ax.clear()
        img_data = self.pix.tobytes("png")
        import io
        from PIL import Image
        img = Image.open(io.BytesIO(img_data))
        self.ax.imshow(img)
        self.ax.set_title(f"Pagina {self.current_page + 1}/{len(self.doc)} - Trascina per creare zona", 
                         fontsize=12, weight='bold')
        
        # Ridisegna zone della pagina corrente
        for zone in self.zones:
            if self._zone_applies_to_page(zone, self.current_page):
                self._draw_zone(zone)
    
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
    
    def _draw_zone(self, zone):
        """Disegna una zona esistente"""
        x_px, y_px = self.pdf_to_pixel(zone['x'], zone['y'])
        w_px = zone['width'] * self.pix.width / self.page_width
        h_px = zone['height'] * self.pix.height / self.page_height
        
        rect = Rectangle((x_px, y_px), w_px, h_px,
                        linewidth=2, edgecolor='green',
                        facecolor='green', alpha=0.2)
        self.ax.add_patch(rect)
        
        # Etichetta con info pagine
        pages_info = self._format_pages_info(zone.get('pages', 'current'))
        label_text = f"{zone['label']}\n{pages_info}"
        
        self.ax.text(x_px + w_px/2, y_px + h_px/2, label_text,
                    ha='center', va='center', color='darkgreen',
                    fontsize=9, weight='bold',
                    bbox=dict(boxstyle='round', facecolor='white', alpha=0.8))
    
    def _format_pages_info(self, pages):
        """Formatta info pagine per visualizzazione"""
        if pages == 'all':
            return "(tutte)"
        elif isinstance(pages, list):
            if len(pages) == 1:
                return f"(pag. {pages[0] + 1})"
            else:
                page_nums = [str(p + 1) for p in pages]
                return f"(pag. {','.join(page_nums)})"
        elif isinstance(pages, int):
            return f"(pag. {pages + 1})"
        return ""
    
    def _check_duplicate_name(self, label):
        """Verifica se il nome zona esiste già"""
        existing_names = [z['label'] for z in self.zones]
        return label in existing_names
    
    def _suggest_next_name(self):
        """Suggerisce il prossimo nome disponibile"""
        self.zone_counter += 1
        return f"zona_{self.zone_counter}"
    
    def _update_zones_list(self):
        """Aggiorna lista zone nel pannello laterale"""
        if not self.zones:
            self.zones_list_text.set_text("ZONE DEFINITE:\n(nessuna)")
        else:
            text = "ZONE DEFINITE:\n"
            for i, zone in enumerate(self.zones, 1):
                pages_info = self._format_pages_info(zone['pages'])
                text += f"{i}. {zone['label']} {pages_info}\n"
        self.zones_list_text.set_text(text)
    
    def pixel_to_pdf(self, x_px, y_px):
        x_pdf = x_px * self.page_width / self.pix.width
        y_pdf = self.page_height - (y_px * self.page_height / self.pix.height)
        return x_pdf, y_pdf
    
    def pdf_to_pixel(self, x_pdf, y_pdf):
        x_px = x_pdf * self.pix.width / self.page_width
        y_px = (self.page_height - y_pdf) * self.pix.height / self.page_height
        return x_px, y_px
    
    def update_label(self, text):
        self.zone_label = text
    
    def update_pages_mode(self, label):
        modes = {
            'Solo questa pagina': 'current',
            'Tutte le pagine': 'all',
            'Range personalizzato': 'range'
        }
        self.zone_pages_mode = modes[label]
    
    def prev_page(self, event):
        if self.current_page > 0:
            self.current_page -= 1
            self._load_page()
            self._render_page()
            self._update_info()
            self.fig.canvas.draw()
    
    def next_page(self, event):
        if self.current_page < len(self.doc) - 1:
            self.current_page += 1
            self._load_page()
            self._render_page()
            self._update_info()
            self.fig.canvas.draw()
    
    def on_press(self, event):
        if event.inaxes != self.ax:
            return
        self.drawing = True
        self.rect_start = (event.xdata, event.ydata)
    
    def on_motion(self, event):
        if not self.drawing or event.inaxes != self.ax:
            return
        
        if self.current_rect:
            self.current_rect.remove()
        
        x0, y0 = self.rect_start
        width = event.xdata - x0
        height = event.ydata - y0
        
        self.current_rect = Rectangle((x0, y0), width, height,
                                     linewidth=2, edgecolor='red',
                                     facecolor='red', alpha=0.3)
        self.ax.add_patch(self.current_rect)
        
        x_pdf_start, y_pdf_start = self.pixel_to_pdf(x0, y0)
        x_pdf_end, y_pdf_end = self.pixel_to_pdf(event.xdata, event.ydata)
        
        self.info_text.set_text(
            f"Disegno zona '{self.zone_label}'...\n"
            f"PDF: X={min(x_pdf_start, x_pdf_end):.1f}, "
            f"Y={min(y_pdf_start, y_pdf_end):.1f}\n"
            f"W={abs(x_pdf_end - x_pdf_start):.1f}, "
            f"H={abs(y_pdf_end - y_pdf_start):.1f}"
        )
        
        self.fig.canvas.draw_idle()
    
    def on_release(self, event):
        if not self.drawing or event.inaxes != self.ax:
            return
        
        self.drawing = False
        
        x0, y0 = self.rect_start
        x1, y1 = event.xdata, event.ydata
        
        x_pdf_0, y_pdf_0 = self.pixel_to_pdf(x0, y0)
        x_pdf_1, y_pdf_1 = self.pixel_to_pdf(x1, y1)
        
        # Verifica nome duplicato
        if self._check_duplicate_name(self.zone_label):
            print(f"\n⚠ WARNING: Nome '{self.zone_label}' già esistente!")
            suggested = self._suggest_next_name()
            print(f"  Suggerimento: usa un nome diverso (es: '{suggested}')")
            print(f"  Zona NON aggiunta. Cambia il nome e riprova.\n")
            
            # Rimuovi rettangolo temporaneo
            if self.current_rect:
                self.current_rect.remove()
                self.current_rect = None
            self.fig.canvas.draw()
            return
        
        # Determina pagine per questa zona
        if self.zone_pages_mode == 'current':
            pages = self.current_page
        elif self.zone_pages_mode == 'all':
            pages = 'all'
        else:  # range
            range_text = self.range_box.text.strip()
            if '-' in range_text:
                try:
                    start, end = map(int, range_text.split('-'))
                    pages = list(range(start - 1, end))  # 0-indexed
                except:
                    print("⚠ Range non valido, uso pagina corrente")
                    pages = self.current_page
            else:
                pages = self.current_page
        
        zone = {
            "label": self.zone_label,
            "x": round(min(x_pdf_0, x_pdf_1), 1),
            "y": round(min(y_pdf_0, y_pdf_1), 1),
            "width": round(abs(x_pdf_1 - x_pdf_0), 1),
            "height": round(abs(y_pdf_1 - y_pdf_0), 1),
            "pages": pages
        }
        
        self.zones.append(zone)
        self._mark_unsaved()
        
        if self.current_rect:
            self.current_rect.set_edgecolor('green')
            self.current_rect.set_alpha(0.2)
        
        # Etichetta
        text_x = (x0 + x1) / 2
        text_y = (y0 + y1) / 2
        pages_info = self._format_pages_info(pages)
        label_text = f"{self.zone_label}\n{pages_info}"
        
        self.ax.text(text_x, text_y, label_text,
                    ha='center', va='center', color='darkgreen',
                    fontsize=9, weight='bold',
                    bbox=dict(boxstyle='round', facecolor='white', alpha=0.8))
        
        self.current_rect = None
        
        print(f"\n✓ Zona aggiunta: '{self.zone_label}' - Pagine: {self._format_pages_info(pages)}")
        
        # AUTO-SUGGERIMENTO PROSSIMO NOME
        next_name = self._suggest_next_name()
        self.zone_label = next_name
        self.label_box.set_val(next_name)
        print(f"  → Prossimo nome suggerito: '{next_name}' (modificalo se preferisci)\n")
        
        self._update_info()
        self._update_zones_list()
        self.fig.canvas.draw()
    
    def _update_info(self):
        zone_count = len(self.zones)
        zones_this_page = sum(1 for z in self.zones if self._zone_applies_to_page(z, self.current_page))
        
        status = "● NON SALVATO" if self.unsaved_changes else "✓ Salvato"
        
        info = f"{status}\n"
        info += f"Pagina: {self.current_page + 1}/{len(self.doc)}\n"
        info += f"Zone totali: {zone_count}\n"
        info += f"Zone in questa pag: {zones_this_page}\n"
        info += f"Dim. pagina: {self.page_width:.0f}×{self.page_height:.0f} pt"
        self.info_text.set_text(info)
    
    def clear_last_zone(self, event):
        if self.zones:
            removed = self.zones.pop()
            self._mark_unsaved()
            print(f"✗ Rimossa zona: '{removed['label']}' (premi 'Salva JSON' per confermare)")
            self._render_page()
            self._update_info()
            self._update_zones_list()
            self.fig.canvas.draw()
        else:
            print("⚠ Nessuna zona da rimuovere")
    
    def clear_all_zones(self, event):
        """Cancella tutte le zone"""
        if self.zones:
            count = len(self.zones)
            self.zones = []
            self.zone_counter = 0  # Reset contatore
            self.zone_label = "zona_1"
            self.label_box.set_val("zona_1")
            self._mark_unsaved()
            print(f"✗ Rimosse tutte le {count} zone (premi 'Salva JSON' per confermare)")
            self._render_page()
            self._update_info()
            self._update_zones_list()
            self.fig.canvas.draw()
        else:
            print("⚠ Nessuna zona da rimuovere")
    
    def show_all_zones(self, event):
        """Mostra elenco di tutte le zone definite"""
        print("\n" + "="*60)
        print("ZONE DEFINITE:")
        print("="*60)
        if not self.zones:
            print("  (nessuna zona definita)")
        else:
            for i, zone in enumerate(self.zones, 1):
                pages_info = self._format_pages_info(zone['pages'])
                print(f"{i}. '{zone['label']}' {pages_info}")
                print(f"   Coord: ({zone['x']}, {zone['y']}) - {zone['width']}×{zone['height']}")
        print("="*60 + "\n")
    
    def save_profile(self, event, silent=False):
        profile = {
            "profile_name": self.profile_name,
            "pdf_file": os.path.basename(self.pdf_path),
            "total_pages": len(self.doc),
            "page_size": {
                "width": self.page_width,
                "height": self.page_height
            },
            "zones": self.zones
        }
        
        filename = f"profile_{self.profile_name}.json"
        with open(filename, 'w', encoding='utf-8') as f:
            json.dump(profile, f, indent=2, ensure_ascii=False)
        
        self._mark_saved()
        
        if not silent:
            print(f"\n{'='*60}")
            print(f"✓ Profilo salvato: {filename}")
            print(f"{'='*60}")
            print(f"Zone totali: {len(self.zones)}")
            for z in self.zones:
                pages_info = self._format_pages_info(z['pages'])
                print(f"  • {z['label']} {pages_info}: ({z['x']}, {z['y']}) {z['width']}×{z['height']}")
            print(f"{'='*60}\n")
        else:
            print(f"💾 Auto-salvato: {filename}")
        
        self._update_info()
    
    def load_profile(self, event):
        filename = f"profile_{self.profile_name}.json"
        if not os.path.exists(filename):
            print(f"✗ File {filename} non trovato")
            return
        
        with open(filename, 'r', encoding='utf-8') as f:
            profile = json.load(f)
        
        self.zones = profile.get('zones', [])
        
        # Aggiorna contatore per suggerimenti
        if self.zones:
            max_num = 0
            for zone in self.zones:
                if zone['label'].startswith('zona_'):
                    try:
                        num = int(zone['label'].split('_')[1])
                        max_num = max(max_num, num)
                    except:
                        pass
            self.zone_counter = max_num
        
        self._mark_saved()
        self._render_page()
        self._update_info()
        self._update_zones_list()
        self.fig.canvas.draw()
        
        print(f"\n✓ Profilo caricato: {filename}")
        print(f"  Zone caricate: {len(self.zones)}")
    
    def on_close(self, event):
        """Gestisce chiusura finestra con warning se modifiche non salvate"""
        if self.unsaved_changes:
            print("\n" + "!"*60)
            print("⚠ ATTENZIONE: Ci sono modifiche non salvate!")
            print("!"*60)
            print("Le zone modificate/cancellate NON sono state salvate nel JSON.")
            print(f"Per salvare: riapri il tool e premi 'Salva JSON'\n")
    
    def show(self):
        plt.show()


def select_pdf_file():
    root = Tk()
    root.withdraw()
    root.attributes('-topmost', True)
    
    file_path = filedialog.askopenfilename(
        title="Seleziona il PDF da calibrare",
        filetypes=[("PDF files", "*.pdf"), ("All files", "*.*")],
        initialdir=os.getcwd()
    )
    
    root.destroy()
    return file_path


if __name__ == "__main__":
    print("\n" + "="*60)
    print("PDF ZONE CALIBRATOR v3.0 - Con validazione nomi")
    print("="*60 + "\n")
    
    pdf_path = select_pdf_file()
    
    if not pdf_path:
        print("✗ Nessun file selezionato. Uscita.")
        exit()
    
    print(f"✓ File selezionato: {pdf_path}\n")
    
    profile_name = input("Nome profilo (default: 'ospedale_esempio'): ").strip()
    if not profile_name:
        profile_name = "ospedale_esempio"
    
    print(f"\nAvvio calibratore per profilo '{profile_name}'...")
    print("\nNUOVE FUNZIONALITÀ v3.0:")
    print("  ✓ Auto-suggerimento nome successivo dopo ogni zona")
    print("  ✓ Blocco nomi duplicati (con warning)")
    print("  ✓ Pannello laterale con elenco zone definite")
    print("  ✓ Reset contatore quando cancelli tutte le zone")
    print("\nUSO:")
    print("  1. Modifica il nome nel campo se vuoi uno personalizzato")
    print("  2. Trascina per disegnare la zona")
    print("  3. Il tool suggerisce automaticamente il prossimo nome")
    print("="*60 + "\n")
    
    try:
        calibrator = PDFZoneCalibrator(pdf_path, profile_name)
        calibrator.show()
    except Exception as e:
        print(f"\n✗ Errore: {e}")