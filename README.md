# DISPOSITIU AUTOMÀTIC PER A LA MESURA DE LA PRESIÓ ARTERIAL

# DESCRIPCIÓ DEL PROJECTE 
Aquest projecte consisteix en el desenvolupament d’un dispositiu automàtic de mesura de la pressió arterial basat en el mètode oscil·lomètric.

El sistema utilitza un microcontrolador ESP32, un sensor de pressió MPX5050GP, una bomba d’aire i una electrovàlvula per inflar i desinflar un braçalet.

El projecte també inclou un programa en Python per visualitzar i processar les dades obtingudes durant la mesura.

# ESTRUCTURA DEL PROJECTE 
- `src/main.cpp` → Codi principal de l’ESP32
- `src/pyfinal.py` → Programa Python per visualització i processament
- `platformio.ini` → Configuració de PlatformIO 

# HARDWARE UTILITZAT 
- ESP32 DevKit V1
- Sensor de pressió MPX5050GP
- Bomba d’aire 12V
- Electrovàlvula 3V
- MOSFET IRL540N
- Amplificador operacional OPA376
- Altres components electrònics utilitzats en el disseny del circuit electrònic. 
- La PCB que s'ha desenvolupat per a aquest projecte
- Braçalet de pressió arterial


# LLIBRERIES UTILITZADES 
#   Arduino / ESP32
    - Arduino.h
    - TFT_eSPI.h

#   Python
    - matplotlib
    - numpy
    - scipy
    - pyserial
    - re

# INSTAL·LACIÓ
### ESP32
1. Obrir el projecte amb PlatformIO.
2. Connectar l’ESP32.
3. Compilar i carregar el programa.

#   Python
1. Instal·lar les llibreries necessàries:

    ```bash
    pip install matplotlib numpy scipy pyserial
    ```
2. Executar el programa 

# FUNCIONAMENT DEL SISTEMA 
1. Python envia el missatge `START` a l’ESP32.
2. L’ESP32 tanca la vàlvula i infla el braçalet.
3. El sensor de pressió captura:
   - pressió lenta (pasbaix)
   - oscil·lacions del pols (pasbanda)
4. Les dades s’envien per comunicació sèrie i es mostren per la pantalla del dispositiu i en una gràfica a temps real.
5. El braçalet s'infla fins que es deixen de detectar oscil·lacions --> INICIEM FASE DE DESINFLAT.
6. El sensor de pressió segueix capturant:
   - pressió lenta (pasbaix)
   - oscil·lacions del pols (pasbanda)
7. Les dades s’envien per comunicació sèrie,  es mostren per la pantalla del dispositiu i en una gràfica a temps real.
8. Python guarda la gràfica en png i calcula:
   - pressió sistòlica
   - pressió diastòlica
   - MAP
9. Es mostren els valors calculats per la pantalla del dispositiu



# Autor

Mar Bulló Porta  

Grau en Enginyeria Biomèdica – Universitat de Girona

Treball Final de Grau

Tutor: Carles Pous Sabadí

Curs 2025/26
