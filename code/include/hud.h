/**
 * @file hud.h
 * @author Anechitei Bianca & Bozomală Ștefan
 * @brief Interfață avansată Head-Up Display (HUD) pentru controlul și monitorizarea robotului WALL-E.
 * 
 * Conține structura HTML, stilizarea CSS și logica JavaScript pentru:
 * - Telemetrie live (senzori ultrasonici și hartă radar).
 * - Control triplu: Tastatură, Gamepad (XBOX/PS) și Comandă Vocală (Web Speech API).
 * - Vizualizarea stării dinamice a robotului și feedback vizual pentru scanări.
 * 
 * --- LOGICĂ CONTROL (CLIENT-SIDE) ---
 * [TASTATURĂ] 
 *  - W, A, S, D: Mișcare (Înainte, Stânga, Înapoi, Dreapta)
 *  - Z: Scanare Servo (Statică) | X: Scanare 360 (Rotație Robot) | M: Mod Labirint
 * [GAMEPAD XBOX] 
 *  - D-Pad / Stick Stânga: Control complet mișcare și direcție
 *  - Buton A (Buton 0): Activare/Dezactivare Comandă Vocală (Toggle)
 *  - Buton Y (Buton 3): Scanare Servo Statică (Ambii Senzori)
 *  - Buton X (Buton 2): Scanare 360 (Rotație Robot)
 *  - Buton B (Buton 1): Activare Mod Labirint (Căutare Ieșire)
 * [VOCE]
 *  - "Înainte"/"Go", "Înapoi"/"Back", "Stânga"/"Left", "Dreapta"/"Right", "Stop"
 *  - "Scanează"/"Scanare" (Servo), "Radar"/"360" (Rotație)
 *  - "Labirint"/"Ieșire"/"Evadare" (Mod Maze)
 */
#ifndef HUD_H
#define HUD_H

#include <Arduino.h>

// --- INTERFAȚA HUD COMPLETĂ ---
// Folosim PROGMEM pentru a stoca acest șir lung în memoria Flash, economisind RAM-ul prețios.
const char HUD_HTML[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
    <meta charset='UTF-8' name='viewport' content='width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no'>
    <title>Robotul WALL-E (proiect SM - Anechitei Bianca și Bozomală Ștefan) </title>
    <style>
        html, body {
            margin: 0; padding: 0; width: 100vw; height: 100vh; overflow: hidden;
            background: #1a1612; color: #d4c5b3; font-family: 'Courier New', Courier, monospace; 
            text-transform: uppercase; box-sizing: border-box;
        }
        .fullscreen-hud {
            display: flex; flex-direction: column; justify-content: space-between;
            align-items: center; height: 100vh; width: 100vw; padding: 3vh 2vw;
            box-sizing: border-box; position: relative;
        }
        h1 {
            font-size: 2.5vh; font-weight: 900; letter-spacing: 3px; color: #f39c12; 
            text-shadow: 3px 3px 0px #3e2723; margin: 0; border-bottom: 4px dashed #f39c12;
            padding-bottom: 5px; line-height: 1; position: relative; z-index: 10;
        }
        .status-bar { display: flex; gap: 10px; justify-content: center; margin-top: 1vh; }
        #gamepad-status {
            font-size: 1.6vh; color: #c0392b; font-weight: bold; letter-spacing: 1px;
            background: #2c1e18; padding: 0.8vh 2vw; border: 2px solid #c0392b; border-radius: 4px;
        }
        #voice-status {
            font-size: 1.6vh; color: #f39c12; font-weight: bold; letter-spacing: 1px;
            background: #2c1e18; padding: 0.8vh 2vw; border: 2px solid #f39c12; border-radius: 4px;
            cursor: pointer; transition: all 0.2s ease;
        }
        #voice-status.active { background: #1e3518; border-color: #2ecc71; color: #2ecc71; }
        #voice-status.active::before { content: '🎤 '; }
        #maze-btn {
            font-size: 1.6vh; color: #e67e22; font-weight: bold; letter-spacing: 1px;
            background: #2c1e18; padding: 0.8vh 2vw; border: 2px solid #e67e22; border-radius: 4px;
            cursor: pointer; transition: all 0.2s ease;
        }
        #maze-btn.active { background: #e67e22; color: #1a1612; }
        .hud-grid {
            display: grid; grid-template-columns: repeat(3, 1fr); gap: 2vh 2vw;
            width: 100%; max-width: 95vw; flex-grow: 1; margin: 3vh 0; align-items: center; position: relative; z-index: 10;
        }
        #b2, #b5 { grid-column: 2; } /* Centrează casetele pe coloana din mijloc */
        /* --- PANOU MAPARE (RADAR) --- */
        .map-section {
            position: absolute; top: 0; left: 0; width: 100vw; height: 100vh;
            background: radial-gradient(circle at center, rgba(30, 30, 30, 0.95) 0%, rgba(0, 0, 0, 0.98) 100%);
            z-index: 1; overflow: hidden;
        }
        #radar-canvas {
            position: absolute; top: 0; left: 50%; height: 100%; aspect-ratio: 1/1; transform: translateX(-50%); z-index: 1;
        }
        .graphic-container {
            position: absolute; top: 50%; left: 50%; 
            transform: translate(-50%, -50%) scale(0.25); /* Robotul este acum mult mai mic */
            width: 100%; height: 100%; 
            display: flex; align-items: center; justify-content: center; position: relative;
            z-index: 5; pointer-events: none;
        }
        /* --------------------------- */

        .robot-body, .robot-tracks {
            transition: transform 0.2s cubic-bezier(0.175, 0.885, 0.32, 1.275);
        }
        .robot-body {
            width: 22vh; height: 22vh; background: #f39c12; border: 4px solid #3e2723; border-radius: 8px;
            box-shadow: inset 0 0 20px #000, 0 8px 25px rgba(0,0,0,0.6);
            animation: pulseBody 2s infinite ease-in-out; z-index: 2; display: flex; align-items: center; justify-content: center;
        }
        #nav-status { font-size: 2.1vh; font-weight: 900; color: #1a1612; letter-spacing: 1px; text-align: center; line-height: 1.4; }
        .robot-tracks { position: absolute; width: 35vh; height: 26vh; display: flex; justify-content: space-between; z-index: 1; }
        .robot-tracks::before, .robot-tracks::after {
            content: ''; width: 5.5vh; height: 26vh; background: #34251c; border: 4px solid #5c4033; border-radius: 6px; box-shadow: inset 0 0 12px #000;
        }
        .steer-left .robot-tracks { animation: spinContinuousLeft 0.8s linear infinite !important; }
        .steer-right .robot-tracks { animation: spinContinuousRight 0.8s linear infinite !important; }
        .scanning .robot-body, .scanning .robot-tracks { animation: heavyRecoilScan 1.5s ease-in-out forwards !important; }
        .sensor-box {
            background: rgba(34, 26, 20, 0.3); border: 2px solid rgba(74, 53, 37, 0.6); border-radius: 8px; padding: 3vh 1vw;
            box-shadow: inset 0 0 15px rgba(0,0,0,0.9); display: flex; flex-direction: column; justify-content: center; align-items: center; height: 100%; box-sizing: border-box; transition: all 0.3s ease;
        }
        .sensor-box.fwd { border-top: 6px solid #e67e22; }
        .sensor-box.rear { border-bottom: 6px solid #e67e22; }
        .sensor-box.flash { background: #1e3518; border-color: #2ecc71; }
        .sensor-box.alert { background: #4a1515 !important; border-color: #ff4444 !important; box-shadow: 0 0 20px #ff4444; }
        .sensor-title { font-size: 1.5vh; color: #8e7a6b; font-weight: 900; margin-bottom: 1vh; letter-spacing: 1px; }
        .val { font-size: 6vh; font-weight: 900; color: #e67e22; font-family: monospace; text-shadow: 0 0 8px rgba(230,126,34,0.3); line-height: 1; }
        .unit { font-size: 2vh; color: #5c4033; font-weight: bold; margin-left: 3px; }
        .moving .robot-tracks::before { animation: trackVibeLeft 0.1s infinite linear; }
        .moving .robot-tracks::after { animation: trackVibeRight 0.1s infinite linear; }
        #scan-alert {
            position: absolute; top: 45%; left: 50%; transform: translate(-50%, -50%) scale(0.7);
            background: rgba(211, 84, 0, 0.95); color: #fff; padding: 2vh 4vw; font-size: 3vh; font-weight: 900; border: 4px solid #fff; border-radius: 8px; box-shadow: 0 0 30px rgba(0,0,0,0.8), 0 0 15px #d35400; z-index: 10; letter-spacing: 2px; opacity: 0; visibility: hidden; transition: all 0.3s cubic-bezier(0.175, 0.885, 0.32, 1.275);
        }
        #scan-alert.show { opacity: 1; visibility: visible; transform: translate(-50%, -50%) scale(1); }
        @keyframes pulseBody { 0% { transform: scale(1); opacity: 0.85; } 50% { transform: scale(1.04); opacity: 1; } 100% { transform: scale(1); opacity: 0.85; } }
        @keyframes trackVibeLeft { 0% { transform: translateY(0px); } 50% { transform: translateY(-5px); } 100% { transform: translateY(0px); } }
        @keyframes trackVibeRight { 0% { transform: translateY(-5px); } 50% { transform: translateY(0px); } 100% { transform: translateY(-4px); } }
        @keyframes spinContinuousLeft { from { transform: rotate(0deg); } to { transform: rotate(-360deg); } }
        @keyframes spinContinuousRight { from { transform: rotate(0deg); } to { transform: rotate(360deg); } }
        @keyframes heavyRecoilScan { 0% { transform: rotate(0deg); } 25% { transform: rotate(-35deg); } 65% { transform: rotate(25deg); } 100% { transform: rotate(0deg); } }
        #console-log {
            width: 100%; max-width: 95vw; text-align: left; padding: 1vh; background: #0d0a08; font-family: monospace; font-size: 1.3vh; color: #524339; height: 9vh; overflow-y: auto; border-top: 2px dashed #221a14; box-sizing: border-box; flex-shrink: 0;
        }
    </style>
</head>
<body>
    <div class="fullscreen-hud">
        <div id="scan-alert">⚠️ SCANARE ACTIVĂ ⚠️</div>

        <!-- RADAR FULLSCREEN ÎN FUNDAL -->
        <div class="map-section" id="map-layer">
            <canvas id="radar-canvas" width="800" height="800"></canvas>
            <div id="robot-graphic" class="graphic-container">
                <div class="robot-tracks"></div>
                <div class="robot-body" title="WALL-E Chassis">
                    <div id="nav-status">STOP</div>
                </div>
            </div>
        </div>

        <div>
            <h1>Robotul WALL-E (proiect SM - Anechitei Bianca și Bozomală Ștefan)</h1>
        </div>
        
        <div class="hud-grid" id="main-grid">
            <!-- RÂNDUL 1: VOCE | SENZOR FAȚĂ | LABIRINT -->
            <div style="display: flex; flex-direction: column; gap: 5px;">
                <div id='voice-status' title='Apasă pentru activare vocală'>COMANDĂ VOCALĂ: INACTIVĂ</div>
                <div id='maze-btn' onclick="runMazeMode()">MOD LABIRINT: OPRIT</div>
            </div>
            
            <div class="sensor-box fwd" id="b2">
                <div class="sensor-title">ÎNAINTE</div>
                <div><span class="val" id="s2">--</span><span class="unit">MM</span></div>
            </div>

            <div id='gamepad-status'>XBOX LINK: OFFLINE</div>

            <!-- SPAȚIU PENTRU VIZIBILITATE RADAR CENTRU -->
            <div style="grid-column: 1 / span 3; height: 55vh;"></div>

            <div class="sensor-box rear" id="b5">
                <div class="sensor-title">ÎNAPOI</div>
                <div><span class="val" id="s5">--</span><span class="unit">MM</span></div>
            </div>
        </div>
        <div id="console-log">System telemetry active. Ready for test inputs...</div>
    </div>

    <script>
        // --- VARIABILE GLOBALE ---
        let currentCmd = 'stop';
        let isScanning = false;
        let lastButton0State = false; // Memorie pentru starea butonului A (Voce)
        let lastButton1State = false; // Memorie pentru starea butonului B (Maze)
        let lastButton2State = false; // Memorie pentru starea butonului X (Scan 360)
        let lastButton3State = false; // Memorie pentru starea butonului Y (Servo)
        let recognition = null;       // Obiectul pentru recunoaștere vocală
        let voiceStatusElement = null; // Elementul UI pentru starea vocii

        // --- TELEMETRIE ȘI ACTUALIZARE RADAR ---
        setInterval(async () => {
            if (isScanning) return;
            try {
                const res = await fetch('/data');
                const d = await res.json();
                document.getElementById('s2').innerText = d.f;
                document.getElementById('s5').innerText = d.b;
                document.getElementById('b2').classList.toggle('alert', d.f < 150);
                document.getElementById('b5').classList.toggle('alert', d.b < 150);

                /**
                 * Actualizare vizuală a hărții radar.
                 * Primește un array de 72 de puncte (5 grade fiecare) reprezentând distanțele în mm.
                 */
                if(d.map) updateRadarMap(d.map);
            } catch(e){}
        }, 200);

        // --- LOGICĂ DESENARE HARTĂ RADAR (ENHANCED SONAR) ---
        function updateRadarMap(points) {
            const canvas = document.getElementById('radar-canvas');
            const ctx = canvas.getContext('2d');
            const center = canvas.width / 2;
            const maxDist = 1500; // Raza maximă vizibilă pe hartă (1.5 metri)
            const scale = (canvas.width / 2) / maxDist;

            ctx.clearRect(0, 0, canvas.width, canvas.height);

            // --- DESENARE GRID-UL POLAR (VERDE DIN REFERINȚĂ) ---
            
            // Cercuri concentrice (Grid radial)
            const gridColor = "#2ecc71"; // Verde deschis (match reference)
            const gridAlpha = 0.25;
            ctx.strokeStyle = gridColor;
            ctx.globalAlpha = gridAlpha;
            ctx.lineWidth = 2;
            [500, 1000, 1500].forEach(d => {
                ctx.beginPath();
                ctx.arc(center, center, d * scale, 0, Math.PI * 2);
                ctx.stroke();
            });

            // Liniile radiale (Spokes) - 12 linii pentru 8 direcții principale
            ctx.globalAlpha = gridAlpha * 0.7;
            ctx.lineWidth = 1.5;
            for (let angle = 0; angle < 360; angle += 30) {
                const rad = (angle - 90) * (Math.PI / 180);
                const x1 = center + Math.cos(rad) * maxDist * scale;
                const y1 = center + Math.sin(rad) * maxDist * scale;
                ctx.beginPath();
                ctx.moveTo(center, center);
                ctx.lineTo(x1, y1);
                ctx.stroke();
            }

            // Etichetele direcțiilor (N, E, S, W)
            ctx.globalAlpha = 0.6;
            ctx.fillStyle = gridColor;
            ctx.font = "bold 16px monospace";
            ctx.textAlign = "center";
            ctx.textBaseline = "middle";
            const labelDist = maxDist * scale + 30;
            ['N', 'E', 'S', 'W'].forEach((label, i) => {
                const angle = (i * 90 - 90) * (Math.PI / 180);
                const x = center + Math.cos(angle) * labelDist;
                const y = center + Math.sin(angle) * labelDist;
                ctx.fillText(label, x, y);
            });

            // --- DESENARE CERCUL CENTRAL (ROBOTUL) ---
            ctx.globalAlpha = 0.8;
            ctx.fillStyle = "#f39c12";
            ctx.beginPath();
            ctx.arc(center, center, 8, 0, Math.PI * 2);
            ctx.fill();

            // --- DESENARE OBSTACOLELE (PUNCTE RADAR) ---
            ctx.globalAlpha = 1;
            points.forEach((dist, i) => {
                if (dist > 0 && dist < 2000) {
                    // Conversie Polar -> Cartezian
                    // Aliniere: index 18 (Forward) trebuie să fie la -90 grade (North)
                    let angle = (i * 5 - 180) * (Math.PI / 180);
                    let x = center + (dist * scale) * Math.cos(angle);
                    let y = center + (dist * scale) * Math.sin(angle);

                    // Glow effect mare (halo exterior)
                    const gradient = ctx.createRadialGradient(x, y, 0, x, y, 12);
                    gradient.addColorStop(0, "rgba(46, 204, 113, 0.6)");
                    gradient.addColorStop(1, "rgba(46, 204, 113, 0)");
                    ctx.fillStyle = gradient;
                    ctx.beginPath();
                    ctx.arc(x, y, 12, 0, Math.PI * 2);
                    ctx.fill();

                    // Punctul principal (mai luminos)
                    ctx.fillStyle = "#2ecc71";
                    ctx.beginPath();
                    ctx.arc(x, y, 6, 0, Math.PI * 2);
                    ctx.fill();

                    // Centrul punctului (glow central)
                    ctx.fillStyle = "#fff";
                    ctx.globalAlpha = 0.8;
                    ctx.beginPath();
                    ctx.arc(x, y, 2.5, 0, Math.PI * 2);
                    ctx.fill();
                    ctx.globalAlpha = 1;
                }
            });

            ctx.globalAlpha = 1;
        }

        /**
         * Trimite o comandă de mișcare către serverul ESP8266.
         * @param {string} cmd - Comanda ('go', 'back', 'left', 'right', 'stop').
         */
        function sendCmd(cmd) {
            if (isScanning) return; 
            let label = cmd.toUpperCase();
            let navText = 'STOP';
            let container = document.getElementById('robot-graphic');
            container.classList.remove('steer-left', 'steer-right');
            if(label === 'GO')    { navText = 'ÎNAINTE<br>⬆️'; }
            if(label === 'BACK')  { navText = 'MARȘARIER<br>⬇️'; }
            if(label === 'LEFT')  { navText = 'ROTIRE STÂNGA<br>↪️'; container.classList.add('steer-left'); }
            if(label === 'RIGHT') { navText = 'ROTIRE DREAPTA<br>↩️'; container.classList.add('steer-right'); }
            if(label === 'STOP')  { navText = 'PARCAT<br>🅿️'; }
            document.getElementById('nav-status').innerHTML = navText;
            if(cmd !== 'stop') container.classList.add('moving');
            else container.classList.remove('moving');
            fetch('/' + cmd);
            const log = document.getElementById('console-log');
            log.innerHTML += `<br>[SYS-LINK] Send HTTP GET /${cmd} -> 200 OK`;
            log.scrollTop = log.scrollHeight;
        }

        /**
         * Execută scanarea statică (doar cu servo).
         * Trimite cererea către server și actualizează UI-ul pe durata procesului.
         */
        function runComprehensiveScan() {
            if (isScanning) return;
            isScanning = true;
            currentCmd = 'stop';
            let container = document.getElementById('robot-graphic');
            container.classList.remove('moving', 'steer-left', 'steer-right');
            const log = document.getElementById('console-log');
            log.innerHTML += `<br>[RADAR] Executing Hardware Servo Sweep...`;
            document.getElementById('scan-alert').classList.add('show');
            document.getElementById('nav-status').innerHTML = "SCANARE ÎMPREJURIMI<br>🔄";
            container.classList.add('scanning');
            fetch('/scan').then(() => {
                isScanning = false;
                document.getElementById('scan-alert').classList.remove('show');
                container.classList.remove('scanning');
                document.getElementById('nav-status').innerHTML = "STOP";
                [2, 5].forEach(i => document.getElementById('b' + i).classList.add('flash'));
                log.innerHTML += `<br>[RADAR] Hardware sweep complete.`;
                setTimeout(() => [2, 5].forEach(i => document.getElementById('b' + i).classList.remove('flash')), 500);
            }).catch(() => { 
                isScanning = false; 
                document.getElementById('scan-alert').classList.remove('show');
                container.classList.remove('scanning');
                document.getElementById('nav-status').innerHTML = "STOP";
            });
        }

        /**
         * Execută scanarea radar 360 (prin rotația robotului).
         * Robotul se va roti pe loc în pași mici pentru a construi harta.
         */
        function run360Scan() {
            if (isScanning) return;
            isScanning = true;
            document.getElementById('scan-alert').classList.add('show');
            const log = document.getElementById('console-log');
            log.innerHTML += `<br>[RADAR] Initiating 360 Full Body Rotation Scan (Step-by-Step, Slower)...`;
            fetch('/scan360').then(() => {
                isScanning = false;
                document.getElementById('scan-alert').classList.remove('show');
                log.innerHTML += `<br>[RADAR] 360 Scan Complete. Map Updated.`;
                [2, 5].forEach(i => {
                    const el = document.getElementById('b' + i);
                    if(el) { el.classList.add('flash'); setTimeout(() => el.classList.remove('flash'), 500); }
                });
            }).catch(() => { 
                isScanning = false; 
                document.getElementById('scan-alert').classList.remove('show');
            });
        }

        /**
         * Activează modul de rezolvare labirint (căutare ieșire).
         */
        function runMazeMode() {
            if (isScanning) return;
            const log = document.getElementById('console-log');
            let btn = document.getElementById('maze-btn');
            let isActive = btn.classList.toggle('active');
            btn.innerText = isActive ? "MOD LABIRINT: ACTIV" : "MOD LABIRINT: OPRIT";
            
            const status = isActive ? 'START' : 'STOP';
            log.innerHTML += `<br>[MAZE] Protocol status: ${status}`;
            document.getElementById('nav-status').innerHTML = isActive ? "CĂUTARE IEȘIRE<br>🔍" : "PARCAT<br>🅿️";
            fetch('/maze').catch(() => {
                log.innerHTML += `<br>[MAZE] Command sent to WALL-E.`;
            });
        }

        // --- CONTROL TASTATURĂ ---
        window.onkeydown = (e) => {
            let k = e.key.toLowerCase();
            if (k === currentCmd) return;
            if(k=='w') { currentCmd = 'w'; sendCmd('go'); } 
            else if(k=='s') { currentCmd = 's'; sendCmd('back'); }
            else if(k=='a') { currentCmd = 'a'; sendCmd('left'); } 
            else if(k=='d') { currentCmd = 'd'; sendCmd('right'); }
            else if(k=='z') { runComprehensiveScan(); }
            else if(k=='x') { run360Scan(); }
            else if(k=='m') { runMazeMode(); }
        };
        window.onkeyup = (e) => { 
            if(['w','s','a','d'].includes(e.key.toLowerCase())) {
                currentCmd = 'stop';
                sendCmd('stop');
            } 
        };

        window.addEventListener('gamepadconnected', (e) => {
            let status = document.getElementById('gamepad-status');
            /**
             * Detectează conectarea unui controller. 
             * Standardul Gamepad API mapează butoanele pentru controllere XBOX/PlayStation
             * într-un format indexat universal.
             */
            status.innerText = 'XBOX LINK: TELEMETRY ONLINE ✅';
            status.style.color = '#f39c12'; status.style.borderColor = '#f39c12'; status.style.background = '#3e2723';
        });

        window.addEventListener('gamepaddisconnected', () => {
            let status = document.getElementById('gamepad-status');
            status.innerText = 'XBOX LINK: OFFLINE';
            status.style.color = '#c0392b'; status.style.border = '1px solid #c0392b'; status.style.background = '#2c1e18';
        });
        
        /**
         * Loop de monitorizare a stării Gamepad-ului (20Hz).
         * Gestionează mișcarea analogică și declanșarea funcțiilor speciale prin butoane.
         */
        setInterval(() => {
            const gamepads = navigator.getGamepads();
            if (!gamepads) return;
            let gp = null;
            for (let i = 0; i < gamepads.length; i++) { if (gamepads[i]) { gp = gamepads[i]; break; } }
            if (!gp) return;

            // Gestionare Buton A (Voce) - Toggle cu protecție la repetiție
            let b0 = gp.buttons[0].pressed;
            if (b0 && !lastButton0State && recognition && voiceStatusElement) {
                if (voiceStatusElement.classList.contains('active')) recognition.stop();
                else recognition.start();
            }
            lastButton0State = b0;

            // Gestionare Buton B (index 1) -> Mod Labirint
            let b1 = gp.buttons[1].pressed;
            if (b1 && !lastButton1State) runMazeMode();
            lastButton1State = b1;

            // Gestionare Buton Y (Scanare Servo)
            let b3 = gp.buttons[3].pressed;
            if (b3 && !lastButton3State) runComprehensiveScan();
            lastButton3State = b3;

            // Gestionare Buton X (index 2) -> Scanare 360 cu debouncing
            let b2 = gp.buttons[2] ? gp.buttons[2].pressed : false;
            if (b2 && !lastButton2State && !isScanning) run360Scan();
            lastButton2State = b2;

            if (isScanning) return; 
            let cmd = 'stop';
            
            // Prioritate 1: D-Pad
            if (gp.buttons[12] && gp.buttons[12].pressed) cmd = 'go';       
            else if (gp.buttons[13] && gp.buttons[13].pressed) cmd = 'back'; 
            else if (gp.buttons[14] && gp.buttons[14].pressed) cmd = 'left'; 
            else if (gp.buttons[15] && gp.buttons[15].pressed) cmd = 'right';

            // Prioritate 2: Analog Sticks (LS și RS)
            if (cmd === 'stop' && gp.axes.length >= 4) {
                // LS: axes[0], axes[1] | RS: axes[2], axes[3]
                let lx = gp.axes[0], ly = gp.axes[1];
                let rx = gp.axes[2], ry = gp.axes[3];

                // Combinăm valorile (folosim stick-ul care are deviația mai mare)
                let x = Math.abs(lx) > Math.abs(rx) ? lx : rx;
                let y = Math.abs(ly) > Math.abs(ry) ? ly : ry;

                // Prag de sensibilitate (deadzone) de 0.5
                if (y < -0.5) cmd = 'go';       
                else if (y > 0.5) cmd = 'back'; 
                else if (x < -0.5) cmd = 'left';  
                else if (x > 0.5) cmd = 'right'; 
            }
            if (cmd !== currentCmd && currentCmd !== 'w' && currentCmd !== 's' && currentCmd !== 'a' && currentCmd !== 'd') {
                currentCmd = cmd;
                sendCmd(cmd);
            }
        }, 50);

        // --- COMANDĂ VOCALĂ ---
        const SpeechRecognition = window.SpeechRecognition || window.webkitSpeechRecognition;
        voiceStatusElement = document.getElementById('voice-status');
        /**
         * Inițializează recunoașterea vocală folosind browser-ul clientului.
         * Procesează sunetul local și trimite doar comenzile text către robot.
         */
        let log = document.getElementById('console-log');

        if (SpeechRecognition) {
            recognition = new SpeechRecognition();
            recognition.continuous = false; // Ascultă o singură comandă, apoi se oprește
            recognition.lang = 'ro-RO';     // Setează limba la Română
            recognition.interimResults = false; // Nu afișa rezultate intermediare
            recognition.maxAlternatives = 1;    // Doar cea mai bună potrivire

            recognition.onstart = () => {
                voiceStatusElement.innerText = 'COMANDĂ VOCALĂ: ACTIVĂ';
                voiceStatusElement.classList.add('active');
                log.innerHTML += `<br>[VOCE] Ascult...`;
                log.scrollTop = log.scrollHeight;
            };

            recognition.onresult = (event) => {
                const transcript = event.results[0][0].transcript.toLowerCase();
                log.innerHTML += `<br>[VOCE] Recunoscut: "${transcript}"`;
                log.scrollTop = log.scrollHeight;
                processVoiceCommand(transcript);
            };

            recognition.onerror = (event) => {
                log.innerHTML += `<br>[VOCE] Eroare: ${event.error}`;
                log.scrollTop = log.scrollHeight;
                voiceStatusElement.innerText = 'COMANDĂ VOCALĂ: INACTIVĂ';
                voiceStatusElement.classList.remove('active');
            };

            recognition.onend = () => {
                voiceStatusElement.innerText = 'COMANDĂ VOCALĂ: INACTIVĂ';
                voiceStatusElement.classList.remove('active');
                log.innerHTML += `<br>[VOCE] Ascultare oprită.`;
                log.scrollTop = log.scrollHeight;
            };

            voiceStatusElement.onclick = () => {
                if (voiceStatusElement.classList.contains('active')) {
                    recognition.stop();
                } else {
                    recognition.start();
                }
            };
        } else {
            voiceStatusElement.innerText = 'COMANDĂ VOCALĂ: NESUPORTATĂ';
            voiceStatusElement.style.color = '#c0392b';
            voiceStatusElement.style.borderColor = '#c0392b';
        }

        /**
         * Interpretează textul recunoscut și execută acțiunea corespunzătoare.
         * @param {string} command - Textul procesat de Web Speech API.
         */
        function processVoiceCommand(command) {
            if (command.includes('înainte') || command.includes('go')) sendCmd('go');
            else if (command.includes('înapoi') || command.includes('back')) sendCmd('back');
            else if (command.includes('stânga') || command.includes('left')) sendCmd('left');
            else if (command.includes('dreapta') || command.includes('right')) sendCmd('right');
            else if (command.includes('stop') || command.includes('oprește')) sendCmd('stop');
            else if (command.includes('scanează') || command.includes('scanare')) runComprehensiveScan();
            else if (command.includes('radar') || command.includes('360')) run360Scan();
            else if (command.includes('labirint') || command.includes('ieșire') || command.includes('evadare')) runMazeMode();
            else log.innerHTML += `<br>[VOCE] Comandă necunoscută: "${command}"`;
        }

        // Desenează grila radarului imediat la încărcare
        updateRadarMap(new Array(72).fill(0));
    </script>
</body>
</html>
)=====";

#endif