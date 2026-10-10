/**
 * @file main.cpp
 * @author Anechitei Bianca & Bozomală Ștefan - Anul 2, Grupa 30126, Semigrupa 1, Facultatea de Automatică și Calculatoare, Specializarea Automatică și Informatică Aplicată, Universitatea Tehnica din Cluj-Napoca.
 * @brief Nucleul logic și serverul de control pentru robotul autonom/telecomandat WALL-E.
 * @version 1.2
 * 
 * Funcționalități incluse:
 * - Conectivitate Wi-Fi (Mod Stație/Hotspot) și server web asincron.
 * - Control PWM avansat cu rampă de accelerație și decelerare lină.
 * - Sistem de siguranță activă cu frânare proporțională în funcție de distanță.
 * - Scanare radar 360° (cinematică) și scanare statică cu servo-motor.
 * - Serviciu mDNS pentru acces facil via http://walle.local. // Permite accesul la interfața web - CTRL+CLICK pe link pentru a accesa interfața de control.
 * 
 * --- INSTRUCȚIUNI CONTROL (HUD) ---
 * [TASTATURĂ]
 *  - W: Înainte | S: Înapoi | A: Rotire Stânga | D: Rotire Dreapta
 *  - Z: Scanare Servo Statică (Sweep)
 *  - X: Scanare Radar 360 (Rotație Robot)
 * [GAMEPAD / XBOX LINK]
 *  - Stick Stânga/Dreapta / D-Pad: Control direcție și mișcare
 *  - Buton A (Buton 0): Activare/Dezactivare Comandă Vocală (Toggle)
 *  - Buton B (Buton 1): Activare Mod Labirint (Căutare Ieșire)
 *  - Buton Y (Buton 3): Scanare Servo Statică (Sweep - Ambii Senzori)
 *  - Buton X (Buton 2): Scanare Radar 360 (Rotație Robot)
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Servo.h>
#include <ESP8266mDNS.h>
#include <NewPing.h>
#include "hud.h" // Includem interfața HTML separată

// --- ALOCARE PINI (Specificații Wemos D1 R2) ---
// Motoare (Driver L298N)
#define ENABLE_PIN 16 // Pin D0 - Activare generală driver
#define IN1_PIN 2     // Pin D4 - Motor Stânga Direcție 1
#define IN2_PIN 14    // Pin D5 - Motor Stânga Direcție 2
#define IN3_PIN 12    // Pin D6 - Motor Dreapta Direcție 1
#define IN4_PIN 13    // Pin D7 - Motor Dreapta Direcție 2

// Senzori și Actuatori (HC-SR04 & Servo)
#define SERVO_PIN 4   // Pin D2 - Control unghi servomotor
#define TRIG_FATA 15  // Pin D8 - Trigger Senzor Ultrasonic Față
#define ECHO_FATA 5   // Pin D1 - Echo Senzor Ultrasonic Față
#define TRIG_SPATE 1  // Pin TX (GPIO1) - Trigger Senzor Ultrasonic Spate
#define ECHO_SPATE 3  // Pin RX (GPIO3) - Echo Senzor Ultrasonic Spate

// --- CONFIGURARE WI-FI ---
/** @brief SSID-ul rețelei Wi-Fi la care se va conecta robotul */
#define WIFI_SSID "introdu_ssid_tau_aici" // Înlocuiește cu SSID-ul (Numele retelei) tău real // Noi am folosit hotspotul telefonului - robotul se conecta la el, precum si laptopul pentru a accesa interfața web și a trimite comenzi.
/** @brief Parola rețelei Wi-Fi */
#define WIFI_PASS "introdu_parola_ta_aici" // Înlocuiește cu parola ta reală

// --- CONSTANTE PARAMETRI MIȘCARE ---
const int PWM_MERS = 1023;      // Viteza maximă de deplasare (10-bit PWM)
const int PWM_ROTIRE = 850;     // Putere necesară pentru a învinge inerția la rotație
const int PWM_MINIM = 320;      // Tensiunea minimă la care motoarele încep să se miște
/** @brief Distanța minimă de siguranță în mm pentru oprire forțată */
const int LIMITA_STOP_MM = 200; // 200mm = 20cm (conform cerință siguranță)
/** @brief Pragul de la care începe decelerarea progresivă (mm) */
const int DIST_SLOW_MM = 500;  // Începe decelerarea de la 50cm pentru o frânare lină

// --- CONSTANTE EVITARE OBSTACOLE (MAZE SOLVER) ---
#define UNGHI_STANGA    150
#define UNGHI_FATA      90
#define UNGHI_DREAPTA   30
#define UNGHI_FATA_ST   120
#define UNGHI_FATA_DR   60
#define DIST_LIBERA_MM  400   // 40cm - prag pentru drum liber
#define TIMP_SERVO      350   // ms - stabilizare servo

ESP8266WebServer server(80);
Servo turela;
NewPing sonarFata(TRIG_FATA, ECHO_FATA, 200);   // Senzor ultrasonic față (max 2m)
NewPing sonarSpate(TRIG_SPATE, ECHO_SPATE, 200); // Senzor ultrasonic spate (max 2m)

// --- VARIABILE TELEMETRIE (MM) ---
int distFata = 2000, distSpate = 2000;
int s_stg_f = 2000, s_drp_f = 2000, s_stg_s = 2000, s_drp_s = 2000; 
int radarMap[72];      // Tablou pentru harta 360 (72 slice-uri a câte 5 grade)
int radarScan1[72];    // Prima rotație completă
int radarScan2[72];    // A doua rotație completă
int stareCurenta = 0;  // Stări: 0=Stop, 1=Fata, 2=Spate, 3=Left, 4=Right, 5=MazeSearch, 6=MazeAlign
bool hardwareBusy = false; // Flag pentru a preveni comenzi suprapuse
bool modAutonomAvansatActiv = false; // Flag pentru modul de evadare labirint

// Gestionare viteze pentru rampă (Smooth Start/Stop)
int targetL = 0, targetR = 0;
float currentL = 0, currentR = 0;

// --- FUNCȚIE CALCUL MM REALI ---
/**
 * @brief Citește distanța de la un senzor ultrasonic.
 * @param sonar Referință către obiectul NewPing al senzorului.
 * @return Distanța măsurată în milimetri (limitată la 2000mm).
 */
int getMM(NewPing& sonar) {
    unsigned int uS = sonar.ping(); // Măsurare timp zbor sunet în microsecunde
    if (uS == 0) return 2000;       // Dacă nu primește ecou, presupunem drum liber (2m)
    return (uS * 10) / US_ROUNDTRIP_CM; // Conversie în milimetri
}

/**
 * @brief Calculează viteza ajustată pentru a evita coliziunile.
 * Implementează o funcție de transfer liniară între zona de siguranță și zona de stop.
 * @param dist Distanța curentă față de obstacol (mm).
 * @param maxPWM Viteza dorită de utilizator.
 * @return Viteza recalculată (0 pentru stop, sau valoare între PWM_MINIM și maxPWM).
 */
int getProportionalSpeed(int dist, int maxPWM) {
    if (dist <= LIMITA_STOP_MM) return 0;    // Zona critică: Oprire imediată
    if (dist >= DIST_SLOW_MM) return maxPWM; // Departe -> Viteză maximă
    
    // Interpolare liniară: scade viteza de la maxPWM la PWM_MINIM pe măsură ce se apropie de obstacol
    int sign = (maxPWM > 0) ? 1 : -1;
    int absMax = abs(maxPWM);
    int v_calculat = map(dist, LIMITA_STOP_MM, DIST_SLOW_MM, PWM_MINIM, absMax);
    return v_calculat * sign;
}

/**
 * @brief Trimite semnalele PWM brute către driverul L298N.
 * @param s1 Putere motor stânga (-1023 la 1023).
 * @param s2 Putere motor dreapta (-1023 la 1023).
 */
void applyHardwarePWM(int s1, int s2) {
    s1 = constrain(s1, -1023, 1023); // Asigură valori valide pentru analogWrite (ESP8266)
    s2 = constrain(s2, -1023, 1023);

    // Logică Motor Stânga (IN1, IN2)
    if (s1 >= 0) { analogWrite(IN1_PIN, s1); analogWrite(IN2_PIN, 0); }
    else { analogWrite(IN1_PIN, 0); analogWrite(IN2_PIN, abs(s1)); }

    // Logică Motor Dreapta (IN3, IN4)
    if (s2 >= 0) { analogWrite(IN3_PIN, s2); analogWrite(IN4_PIN, 0); }
    else { analogWrite(IN3_PIN, 0); analogWrite(IN4_PIN, abs(s2)); }
}

/**
 * @brief Actualizează țintele de viteză pentru sistemul de accelerare lină.
 * @param vStanga Viteză țintă stânga.
 * @param vDreapta Viteză țintă dreapta.
 * @param stare Codul stării de mișcare (pentru logică loop).
 */
void drive(int vStanga, int vDreapta, int stare) {
    stareCurenta = stare; // 1=Fata, 2=Spate, 3=Rotire, 0=Stop
    targetL = vStanga;
    targetR = vDreapta;
}

// --- FUNCȚIE CALCUL MEDIA A 2 SCANĂRI ---
/** @brief Filtrează datele radar prin calcularea mediei între două treceri succesive. */
void averageRadarScans() {
    for(int i = 0; i < 72; i++) {
        radarMap[i] = (radarScan1[i] + radarScan2[i]) / 2;
    }
}

// --- FUNCȚIE DELAY NON-BLOCANTĂ PENTRU WIFI ---
/**
 * @brief Așteptare activă care menține serviciile de rețea funcționale.
 * @param ms Durata în milisecunde.
 */
void smartDelay(unsigned long ms) {
    unsigned long start = millis();
    while (millis() - start < ms) {
        server.handleClient();
        MDNS.update();
        yield();
    }
}

// --- FUNCȚIE MAPARE UNIFICATĂ (Servo Static + Radar 360) ---
/**
 * @brief Execută scanarea mediului și stochează datele într-un tablou.
 * @param startAngle Unghiul de început.
 * @param endAngle Unghiul de sfârșit.
 * @param angleStep Pasul de scanare.
 * @param settleTime Timp de așteptare pentru stabilizarea servo-ului.
 * @param useRobotRotation True dacă se folosește rotația caroseriei, False pentru servo.
 * @param destArray Tabloul unde se vor salva distanțele.
 */
void mapDistance(int startAngle, int endAngle, int angleStep, int settleTime, bool useRobotRotation, int* destArray) {
    for (int angle = startAngle; angle <= endAngle; angle += angleStep) {
        if (useRobotRotation) {
            // Rotația robotului calculează unghiul dinamic
            turela.write(90);
            yield();
            server.handleClient();
        } else {
            // Servo-ul se mișcă la unghi static
            turela.write(angle);
            smartDelay(settleTime); // Menține conexiunea activă în timpul așteptării
        }
        
        // Citire distanțe de la ambii senzori
        int distFront = getMM(sonarFata);
        int distBack = getMM(sonarSpate);
        
        // Umplem segmentul de mapă corespunzător pasului (ex: de la 20 la 40 grade)
        for (int s = 0; s < (angleStep / 5); s++) {
            // Calcul dinamic indici pentru vizualizare 360 pe HUD
            int idxF = ((angle) / 5) + s;
            if (idxF >= 0 && idxF < 72) {
                radarMap[idxF] = distFront; // Update live hartă globală
                if (destArray != radarMap) destArray[idxF] = distFront;
            }

            int idxB = ((angle + 180) % 360 / 5) + s;
            if (idxB >= 0 && idxB < 72) {
                radarMap[idxB] = distBack; // Update live hartă globală
                if (destArray != radarMap) destArray[idxB] = distBack;
            }
        }

        // Sincronizare cu variabilele HUD (pentru interfața grafică clasică)
        if (angle == 20)  { s_drp_f = distFront; s_stg_s = distBack; }
        if (angle == 160) { s_stg_f = distFront; s_drp_s = distBack; }
    }
    turela.write(90); // Revine cu senzorul pe centru
}                           

/** @brief Execută scanarea mediului folosind doar servomotorul. */
void executeServoScan() {
    drive(0, 0, 0); // Siguranță: robotul trebuie să stea pe loc
    for(int i=0; i<72; i++) { radarScan1[i]=2000; radarScan2[i]=2000; } // Resetare vectori

    // Prima scanare
    mapDistance(20, 160, 20, 250, false, radarScan1);
    smartDelay(300);
    // A doua scanare
    mapDistance(20, 160, 20, 250, false, radarScan2);
    
    // Calculează media și o salvează în radarMap
    averageRadarScans();
}

// --- FUNCȚIE AUXILIARĂ PENTRU SCANARE 360 (ROTAȚIE ROBOT PAS CU PAS) ---
/** @brief Realizează rotația și colectează date de la ambii senzori pentru precizie dublă. */
void performFullRotationScan(int* destArray) {
    turela.write(90); // Aliniază capul pe centru pentru rotație
    smartDelay(200);
    
    // Scanare pas cu pas: 72 pași de 5 grade fiecare
    for(int step = 0; step < 72; step++) {
        // Rotește robotul pas cu pas (pe loc) până ajunge la unghiul țintă
        // Pentru fiecare pas de 10 grade, aplică o viteză de rotație scurtă
        targetL = -PWM_ROTIRE; 
        targetR = PWM_ROTIRE;
        stareCurenta = 3;
        applyHardwarePWM(targetL, targetR);
        currentL = targetL;
        currentR = targetR;

        // --- IMPULS ATOMIC ---
        // Folosim delay() scurt pentru a garanta durata impulsului de rotație
        // fără a fi întrerupt de stiva TCP/IP, asigurând pași constanți de 5 grade.
        delay(40);
        
        drive(0, 0, 0);
        applyHardwarePWM(0, 0);
        currentL = 0; // Resetare rampă pentru a preveni saltul la revenirea în loop
        currentR = 0; 
        smartDelay(100); // Așteptare stabilizare inerție
        
        // --- COLECTARE DATE DIN AMBELE DIRECȚII ---
        // Senzor Față: vede direcția 'step'
        int distFront = getMM(sonarFata);
        radarScan1[step] = distFront;

        // Senzor Spate: vede direcția opusă (step + 180 de grade)
        // Aplicăm offset-ul de 8cm (80mm) solicitat
        int distBack = getMM(sonarSpate);
        if (distBack < 2000) {
            distBack = min(2000, distBack + 80); // Corecție: Distanța față de centru = Citire + Offset
        }
        radarScan2[step] = distBack;
        
        // Actualizăm harta parțial în destArray pentru feedback vizual în HUD (opțional)
        destArray[step] = distFront; 

        smartDelay(50);
    }

    // --- CALCUL MEDIE DIAMETRAL OPUSĂ ---
    // După rotația completă, combinăm datele:
    // Direcția i a fost văzută de Față la pasul i și de Spate la pasul (i + 36) % 72
    for(int i = 0; i < 72; i++) {
        int backIdx = (i + 36) % 72; // Indicele pasului unde spatele era orientat spre direcția i
        destArray[i] = (radarScan1[i] + radarScan2[backIdx]) / 2;
    }
    
    // Nu se mai întoarce la poziția inițială - continuă pentru următoarea scanare
    drive(0, 0, 0); // Doar oprește robotul, nu se mai rotește
    hardwareBusy = false;
}

/** @brief Execută o scanare radar completă de 360 grade prin rotirea întregului robot. */
void executeFullRobotScan() {
    hardwareBusy = true;
    // Executăm o singură rotație completă 360° (72 de pași a câte 5 grade).
    // Această metodă oferă o hartă mult mai precisă a camerei decât scanarea servo.
    // Datele sunt salvate direct în radarMap pentru a fi afișate imediat în HUD
    performFullRotationScan(radarMap);
    hardwareBusy = false;
}

/**
 * @brief Funcție autonomă unificată pentru rezolvare labirint:
 * - Scanează 3 puncte (STÂNGA, CENTRU, DREAPTA) cu ambii senzori.
 * - Se rotește pe o singură roată către cea mai mare distanță detectată.
 */
void executeAutonomousMovement() {
    applyHardwarePWM(0, 0);
    drive(0, 0, 0);
    smartDelay(200);

    struct ScanPoint {
        int unghi;
        int distMax;
    };

    ScanPoint scan[3]; 
    int unghiuri[3] = {UNGHI_STANGA, UNGHI_FATA, UNGHI_DREAPTA};

    for (int i = 0; i < 3; i++) {
        turela.write(unghiuri[i]);
        smartDelay(TIMP_SERVO);
        int df = getMM(sonarFata);
        smartDelay(33); // Pauză sigură pentru disiparea ecoului ultrasonic
        int db = getMM(sonarSpate);
        scan[i].unghi = unghiuri[i];
        scan[i].distMax = max(df, db);
    }

    turela.write(90); 
    smartDelay(200);

    int maxDist = 0;
    int indexOptim = 1; // Default centru
    for (int i = 0; i < 3; i++) {
        if (scan[i].distMax > maxDist && scan[i].distMax < 2000) {
            maxDist = scan[i].distMax;
            indexOptim = i;
        }
    }

    if (indexOptim == 0) { // Rotire STÂNGA pe roata dreaptă
        applyHardwarePWM(0, PWM_ROTIRE);
        delay(800); 
        applyHardwarePWM(0, 0);
    } else if (indexOptim == 2) { // Rotire DREAPTA pe roata stângă
        applyHardwarePWM(PWM_ROTIRE, 0);
        delay(800); 
        applyHardwarePWM(0, 0);
    }
    smartDelay(300);

    while (modAutonomAvansatActiv) {
        int dFata = getMM(sonarFata);
        if (dFata <= LIMITA_STOP_MM) {
            applyHardwarePWM(0, 0);
            drive(0, 0, 0);
            smartDelay(500);
            break; 
        }

        int v = getProportionalSpeed(dFata, PWM_MERS);
        applyHardwarePWM(v, v);
        drive(v, v, 1);

        unsigned long startInner = millis();
        while(millis() - startInner < 50) {
            server.handleClient();
            MDNS.update();
            yield();
        }
        if (stareCurenta == 0) break; 
    }

    if (!modAutonomAvansatActiv) {
        applyHardwarePWM(0, 0);
        drive(0, 0, 0);
    }
}



// --- SETUP ---
void setup() {
    Serial.begin(115200);
    Serial.println("\nWALL-E Booting...");

    // Configurare pini Driver Motoare
    pinMode(ENABLE_PIN, OUTPUT); digitalWrite(ENABLE_PIN, HIGH);
    pinMode(IN1_PIN, OUTPUT); pinMode(IN2_PIN, OUTPUT);
    pinMode(IN3_PIN, OUTPUT); pinMode(IN4_PIN, OUTPUT);
    
    analogWriteRange(1023); // Standardizare PWM
    analogWriteFreq(2000);  // Frecvență mai mare pentru a reduce zgomotul motoarelor

    turela.attach(SERVO_PIN); 
    for(int i=0; i<72; i++) {
        radarMap[i] = 2000;
        radarScan1[i] = 2000;
        radarScan2[i] = 2000;
    }
    turela.write(90); // Poziție centrală senzor

    // Conectare la Hotspot-ul telefonului
    WiFi.mode(WIFI_STA); 
    WiFi.setAutoReconnect(true); // Asigură stabilitatea conexiunii pe termen lung
    
    // Pentru hotspot este mai bine să lăsăm DHCP (IP automat) 
    // deoarece telefoanele au subnet-uri diferite.

    WiFi.begin(WIFI_SSID, WIFI_PASS);

    // Așteaptă conectarea (robotul va fi inactiv până se conectează)
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("\nWiFi Connected. IP: " + WiFi.localIP().toString());
    // ELIBERARE PINI TX/RX: Oprim Serial pentru a permite funcționarea corectă a sonarului spate
    Serial.end(); 

    // Pornire serviciu mDNS - permite accesul via http://walle.local
    if (MDNS.begin("walle")) {
        MDNS.addService("http", "tcp", 80);
    }

    // Configurare Rute Server Web
    server.on("/", []() { server.send_P(200, "text/html", HUD_HTML); });
    
    /** @brief Endpoint JSON pentru telemetrie în timp real (distante, harta radar) */
    server.on("/data", []() {
        String json = "{";
        json.reserve(1024); 
        // Adăugăm distanțele principale
        json.concat("\"st\":"); json.concat(stareCurenta);
        json.concat(",");
        json.concat("\"f\":"); json.concat(distFata);
        json.concat(",\"b\":"); json.concat(distSpate);
        // Adăugăm punctele pentru HUD-ul grafic (radar)
        json.concat(",\"sf1\":"); json.concat(s_stg_f);
        json.concat(",\"sf3\":"); json.concat(s_drp_f);
        json.concat(",\"ss4\":"); json.concat(s_stg_s);
        json.concat(",\"ss6\":"); json.concat(s_drp_s);
        json.concat(",\"map\":[");
        for(int i=0; i<72; i++) {
            json.concat(radarMap[i]);
            if(i < 71) json.concat(",");
        }
        json.concat("]}");
        server.send(200, "application/json", json);
    });

    // Comenzi simple: Fata, Spate, Rotire pe loc Stanga/Dreapta, Stop
    server.on("/go", []() { modAutonomAvansatActiv = false; hardwareBusy = false; drive(PWM_MERS, PWM_MERS, 1); server.send(200); });
    server.on("/back", []() { modAutonomAvansatActiv = false; hardwareBusy = false; drive(-PWM_MERS, -PWM_MERS, 2); server.send(200); });
    server.on("/left", []() { modAutonomAvansatActiv = false; hardwareBusy = false; drive(-PWM_ROTIRE, PWM_ROTIRE, 3); server.send(200); });
    server.on("/right", []() { modAutonomAvansatActiv = false; hardwareBusy = false; drive(PWM_ROTIRE, -PWM_ROTIRE, 4); server.send(200); });
    server.on("/stop", []() { modAutonomAvansatActiv = false; hardwareBusy = false; drive(0, 0, 0); server.send(200); });

    server.on("/maze", []() {
        if (modAutonomAvansatActiv) {
            modAutonomAvansatActiv = false;
            hardwareBusy = false;
            drive(0, 0, 0);
        } else {
            if(!hardwareBusy) {
                modAutonomAvansatActiv = true;
                hardwareBusy = true;
                stareCurenta = 5; 
                turela.write(90);
            }
        }
        server.send(200);
    });

    server.on("/scan", []() {
        if(!hardwareBusy) { hardwareBusy = true; executeServoScan(); hardwareBusy = false; }
        server.send(200);
    });

    server.on("/scan360", []() {
        executeFullRobotScan();
        server.send(200);
    });

    server.begin();
}

// --- LOOP ---
void loop() {
    MDNS.update();         // Menține serviciul mDNS activ pentru http://walle.local
    server.handleClient(); // Procesare cereri Wi-Fi
    
    // === MOD AUTONOM AVANSAT ===
    if (modAutonomAvansatActiv) {
        executeAutonomousMovement();
        return;
    }

    static unsigned long lastPing = 0;
    if (millis() - lastPing > 100) { 
        // Citire senzori distanță (10Hz) pentru siguranță activă
        distFata = getMM(sonarFata);
        delay(30); // Pauză între senzori pentru a evita interferența ecoului
        distSpate = getMM(sonarSpate);
        lastPing = millis();
    }

    // --- GESTIONARE MIȘCARE (RAMPĂ ȘI VITEZĂ PROPORȚIONALĂ) ---
    static unsigned long lastMotorUpdate = 0;
    if (millis() - lastMotorUpdate > 30) {
        int finalTargetL = 0;
        int finalTargetR = 0;

        // 1. Viteza adaptivă în funcție de distanță (Safe Driving)
        if (stareCurenta == 1) { // Mergem înainte
            finalTargetL = getProportionalSpeed(distFata, targetL);
            finalTargetR = getProportionalSpeed(distFata, targetR);
        } else if (stareCurenta == 2) { // Mergem înapoi
            finalTargetL = getProportionalSpeed(distSpate, targetL);
            finalTargetR = getProportionalSpeed(distSpate, targetR);
        } else {
            // Fallback pentru STOP (0) sau ROTIRE (3, 4)
            finalTargetL = targetL;
            finalTargetR = targetR;
        }

        // Rezolvare Bug "Sticky Obstacle" global: Dacă senzorii sau logica forțează stop (viteză 0),
        // dar comanda utilizatorului era încă activă, resetăm starea pentru siguranță.
        if ((stareCurenta == 1 || stareCurenta == 2) && finalTargetL == 0 && targetL != 0) {
            drive(0, 0, 0);
        }

        // 2. Rampă de accelerație/decelerare pentru a preveni smuciturile
        const float RAMP_STEP = 25.0; // Ajustează pentru accelerare mai rapidă/lentă
        if (currentL < finalTargetL) currentL = min((float)finalTargetL, currentL + RAMP_STEP);
        else if (currentL > finalTargetL) currentL = max((float)finalTargetL, currentL - RAMP_STEP);

        if (currentR < finalTargetR) currentR = min((float)finalTargetR, currentR + RAMP_STEP);
        else if (currentR > finalTargetR) currentR = max((float)finalTargetR, currentR - RAMP_STEP);

        // 3. Aplicare hardware a vitezei calculate
        applyHardwarePWM((int)currentL, (int)currentR);
        lastMotorUpdate = millis();
    }
}