#include <VPinLedControllerWifi.h>

// --- FEATURE SWITCH ---
#define ENABLE_LED_TEST 1 

// --- LED HARDWARE CONFIG ---
#define FPS_LED_PIN 48
#define FREQ_OUT_PIN 2
#define NUM_STRIPS 16

// --- WLAN ACCESS POINT DATEN ---
// So heißt das WLAN, das der ESP32 ausstrahlt:
const char* ap_ssid = "VPin_LED_Controller";       
const char* ap_password = "vpinpassword";

// Feste IP-Adresse für den AP definieren
IPAddress local_ip(192, 168, 4, 1);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);

// --- DEINE HARDWARE PINS (ESP32-S3) ---
const uint8_t pins[NUM_STRIPS] = {
    1, 4, 5, 6, 7, 8, 9, 10, 11,    // Channels 0 to 7
    12, 13, 14, 15, 16, 17, 18      // Channels 8 to 15
};

// --- 16-CHANNEL ENGINE ---
typedef NeoPixelBus<NeoGrbFeature, NeoEsp32LcdX16Ws2812xMethod> MyPixelBus;
MyPixelBus* strips[NUM_STRIPS];

NeoPixelBus<NeoGrbFeature, NeoEsp32Rmt0Ws2812xMethod> fpsLed(1, FPS_LED_PIN);
uint8_t fpsLedBrightness = 30;

WiFiUDP udp;
uint8_t frameBuffer[36000]; 
uint8_t currentFrameId = 255;
uint8_t chunksReceived = 0;
uint16_t activeLengths[NUM_STRIPS] = {0};
uint8_t pinIndex[NUM_STRIPS];
uint8_t reverseIndex[NUM_STRIPS];

unsigned long lastPacketTime = 0;
bool isStandby = false;
unsigned long lastFpsTime = 0;
int frameCount = 0;

RgbColor applyBrightness(RgbColor color, uint8_t brightness) {
    return RgbColor((color.R * brightness) / 255, (color.G * brightness) / 255, (color.B * brightness) / 255);
}

void ShowAll() {
    for(uint8_t i = 0; i < NUM_STRIPS; i++) {
        strips[i]->Show();
    }
}

void canShow(){
    bool isReady = false;
    while (!isReady){
        isReady = true;
        for(uint8_t i = 0; i < NUM_STRIPS; i++) {
            if (!strips[i]->CanShow()){
                isReady = false;
                break;
            }
        }
    }
}

void UpdateFpsLed() {
    // Diese Funktion wird jetzt direkt in der Hauptschleife aufgerufen, 
    // am besten direkt nach dem Paket-Empfang.
    
        unsigned long currentMillis = millis();
        lastPacketTime = currentMillis;
        frameCount++;
        isStandby = false;

    if (currentMillis - lastFpsTime >= 1000) {
        int currentFps = frameCount;
        
        frameCount = 0;
        lastFpsTime = currentMillis;

        RgbColor fpsColor;

        // Wenn dein 3-Sekunden-Timeout aktiv ist (0 FPS), LED ausmachen oder abdunkeln
        if (currentFps == 0) {
            fpsColor = RgbColor(0, 0, 0); // Aus (Standby)
        } 
        // Die neue, feinere Skala:
        else if (currentFps < 20) {
            fpsColor = RgbColor(255, 0, 0);       // Rot
        } else if (currentFps >= 20 && currentFps <= 29) {
            fpsColor = RgbColor(255, 100, 0);     // Orange
        } else if (currentFps >= 30 && currentFps <= 39) {
            fpsColor = RgbColor(255, 255, 0);     // Gelb (Aktuelles WLAN)
        } else if (currentFps >= 40 && currentFps <= 49) {
            fpsColor = RgbColor(100, 255, 0);     // Hellgrün
        } else if (currentFps >= 50 && currentFps <= 59) {
            fpsColor = RgbColor(0, 255, 0);       // Dunkelgrün
        } else if (currentFps >= 60 && currentFps <= 69) {
            fpsColor = RgbColor(0, 255, 255);     // Cyan (Perfekt)
        } else if (currentFps >= 70 && currentFps <= 89) {
            fpsColor = RgbColor(0, 0, 255);       // Blau
        } else if (currentFps >= 90 && currentFps <= 119) {
            fpsColor = RgbColor(148, 0, 211);     // Violett
        } else { 
            fpsColor = RgbColor(255, 255, 255);   // Weiß (120+)
        }

        fpsLed.SetPixelColor(0, applyBrightness(fpsColor, fpsLedBrightness));
        fpsLed.Show();
    }
}

void ReconfigureLcdDma(uint16_t* newLengths) {
    Serial.println("Neues Frame-Layout erkannt! Starte Hardware-Reset...");

    // 1. RAM freigeben
    for(uint8_t i = 0; i < NUM_STRIPS; i++) {
        if (strips[i] != nullptr) {
            delete strips[i];
            strips[i] = nullptr;
        }
    }

    delay(50); 

    for (int i = 0; i < NUM_STRIPS; i++) {
        pinIndex[i] = i;
        uint16_t exactLen = newLengths[i];
        uint16_t safeLen = (exactLen > 0) ? exactLen : 0;
        activeLengths[i] = exactLen;
    }

    // 2. Sortieren mit std::sort und einer Lambda-Funktion
    std::sort(pinIndex, pinIndex + NUM_STRIPS, [](int a, int b) {
        return activeLengths[a] > activeLengths[b];
    });
    
    for(uint8_t k = 0; k < NUM_STRIPS; k++) {
        // Speichert, an welcher Position 'k' der ursprüngliche Kanal gelandet ist
        reverseIndex[pinIndex[k]] = k; 
    }
    
    // 3. Streifen mit exakter Länge neu aufbauen
    for(uint8_t i = 0; i < NUM_STRIPS; i++) {
        strips[i] = new MyPixelBus(activeLengths[pinIndex[i]], pins[pinIndex[i]]);
        strips[i]->Begin();
        strips[i]->ClearTo(RgbColor(0)); 
    }



    ShowAll();
    Serial.println("Hardware erfolgreich auf neues Tisch-Layout kalibriert!");
    //udp.begin(6454);
    //while(udp.parsePacket()){udp.flush();}
}

void setup() {
    Serial.begin(115200);
    pinMode(FREQ_OUT_PIN, OUTPUT);
    for(uint8_t i = 0; i < NUM_STRIPS; i++) {
        strips[i] = new MyPixelBus(1100, pins[i]);
        strips[i]->Begin();
        strips[i]->ClearTo(RgbColor(0));
    }
    ShowAll();
    
    fpsLed.Begin();

    #if ENABLE_LED_TEST
        uint32_t start = millis();
        while (millis() - start < 500) {
            if (Serial.available() > 0) return;
            yield();
        }
        
        fpsLed.SetPixelColor(0, applyBrightness(RgbColor(255, 255, 255), fpsLedBrightness)); 
        fpsLed.Show();

        for(uint8_t i=0; i<NUM_STRIPS; i++) strips[i]->ClearTo(RgbColor(255, 0, 0));
        ShowAll(); delay(500);
        
        for(uint8_t i=0; i<NUM_STRIPS; i++) strips[i]->ClearTo(RgbColor(0, 255, 0));
        ShowAll(); delay(500);
        
        for(uint8_t i=0; i<NUM_STRIPS; i++) strips[i]->ClearTo(RgbColor(0, 0, 255));
        ShowAll(); delay(500);
        for(uint8_t i=0; i<NUM_STRIPS; i++) strips[i]->ClearTo(RgbColor(0, 0, 0));
        ShowAll();

        fpsLed.SetPixelColor(0, applyBrightness(RgbColor(0, 0, 0), fpsLedBrightness)); 
        fpsLed.Show();

    #endif

    // --- 2. WLAN ACCESS POINT STARTEN ---
    Serial.println("\nStarte Access Point Modus...");

    WiFi.disconnect(true,true);
    // ESP32 in den AP-Modus versetzen
    WiFi.mode(WIFI_AP);
    
    // Feste IP-Adresse zuweisen
    WiFi.softAPConfig(local_ip, gateway, subnet);
    
    // Netzwerk aufspannen
    WiFi.softAP(ap_ssid, ap_password);

    Serial.print("WLAN-Netzwerk '");
    Serial.print(ap_ssid);
    Serial.println("' ist aktiv!");
    Serial.print("IP-Adresse fuer DOF XML: ");
    Serial.println(WiFi.softAPIP());

    // --- 3. UDP STARTEN ---
    udp.begin(6454);
    lastPacketTime = millis();
}

void loop() {
    int packetSize = udp.parsePacket();
    
    if (packetSize > 3) { 
        digitalWrite(FREQ_OUT_PIN, HIGH);
        uint8_t chunkHeader[3];
        udp.read(chunkHeader, 3);
        
        uint8_t fId = chunkHeader[0];
        uint8_t cIdx = chunkHeader[1];
        uint8_t cTotal = chunkHeader[2];
        
        if (fId != currentFrameId) {
            currentFrameId = fId; 
            chunksReceived = 0;   
        }
        
        int offset = cIdx * 1400; 
        int payloadSize = packetSize - 3;
        
        if (offset + payloadSize <= sizeof(frameBuffer)) {
            udp.read(frameBuffer + offset, payloadSize);
            chunksReceived++;
        } else {
            udp.flush();
        }
        
        if (chunksReceived == cTotal) {
            uint16_t stripLengths[NUM_STRIPS];
            bool layoutChanged = false;
            for (int i = 0; i < NUM_STRIPS; i++) {
                stripLengths[i] = (frameBuffer[1 + (i * 2)] << 8) | frameBuffer[2 + (i * 2)];
            
            // Sobald auch nur ein Kanal abweicht, wissen wir: Neues Layout!
                if (stripLengths[i] != activeLengths[i]) {
                    layoutChanged = true;
                }
            
            }

            // 2. Wenn es ein neues Layout ist -> Hardware Reset feuern!
            if (layoutChanged) {
                ReconfigureLcdDma(stripLengths);
            }

            int byteIndex = 33;
            for (int i = 0; i < NUM_STRIPS; i++) {
                uint16_t currentLen = stripLengths[i];
                if (currentLen > 0) {
                    uint8_t channel = reverseIndex[i];
                    for (int len = 0; len < currentLen; len++) {
                        uint8_t r = frameBuffer[byteIndex++];
                        uint8_t g = frameBuffer[byteIndex++];
                        uint8_t b = frameBuffer[byteIndex++];

                        if (len < currentLen) {
                            strips[channel]->SetPixelColor(len, RgbColor(r, g, b));
                        }
                    }
                }
            }
            digitalWrite(FREQ_OUT_PIN, LOW);
            canShow();
            ShowAll();            
            UpdateFpsLed();
            //while(udp.parsePacket()){udp.flush();}
        }
    } 
    else if (!isStandby && (millis() - lastPacketTime > 1500)) {
        for(uint8_t i = 0; i < NUM_STRIPS; i++) {
            strips[i]->ClearTo(RgbColor(0));
        }
        ShowAll();
        
        fpsLed.SetPixelColor(0, RgbColor(0, 0, 0));
        fpsLed.Show();
        frameCount = 0; 
        
        isStandby = true; 
    }
}