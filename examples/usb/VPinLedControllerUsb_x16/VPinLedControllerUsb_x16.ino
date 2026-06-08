#include <VPinLedControllerUsb.h>

#define FirmwareVersionMajor 1
#define FirmwareVersionMinor 3

// --- FEATURE SWITCH ---
#define ENABLE_LED_TEST 1 

// --- LED HARDWARE CONFIG ---
#define FPS_LED_PIN 48
#define FREQ_OUT_PIN 2
#define NUM_STRIPS 16

// --- YOUR PHYSICAL HARDWARE ---
// 16 channels: Enter here exactly the physical lengths of your 16 strips.
const uint16_t stripLengths[NUM_STRIPS] = { 
    144, 144, 0, 0, 0, 0, 0, 0, // Channels 0 to 7
    0, 0, 0, 0, 0, 0, 0, 0  // Channels 8 to 15
};

// --- DEINE HARDWARE PINS (ESP32-S3) ---
const uint8_t pins[NUM_STRIPS] = {
    1, 4, 5, 6, 7, 8, 9, 10, 11,    // Channels 0 to 7
    12, 13, 14, 15, 16, 17, 18      // Channels 8 to 15
};

 // If you send each channel individually, then the maximum number of the longest strip must be smaller. If you send everything over one channel, then the number must be the same or larger than all the LEDs in the system.
const uint16_t dofBlockSize = 1100;
const uint16_t BufferSize = 8192;

// --- 16-CHANNEL ENGINE ---
// We are now using the X16 method of the LCD hardware engine of the ESP32-S3
typedef NeoPixelBus<NeoGrbFeature, NeoEsp32LcdX16Ws2812xMethod> MyPixelBus;
MyPixelBus* strips[NUM_STRIPS];

NeoPixelBus<NeoGrbFeature, NeoEsp32Rmt0Ws2812xMethod> fpsLed(1, FPS_LED_PIN);
uint8_t fpsLedBrightness = 30;

uint8_t rgbDataBuffer[BufferSize]; 
uint16_t ReceivedSize = 0; 

unsigned long lastPacketTime = 0;
bool isStandby = false;
unsigned long lastFpsTime = 0;
int frameCount = 0;

RgbColor applyBrightness(RgbColor color, uint8_t brightness) {
    return RgbColor((color.R * brightness) / 255, (color.G * brightness) / 255, (color.B * brightness) / 255);
}

void Ack() {
    while (!Serial); 
    Serial.write('A');
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

    // Exakt alle 1000 Millisekunden (1 Sekunde) auswerten
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

void setup() {
    Serial.setRxBufferSize(BufferSize);
    Serial.begin(115200);
    pinMode(FREQ_OUT_PIN, OUTPUT);
    Serial.setTimeout(1000); 

    for(uint8_t i = 0; i < NUM_STRIPS; i++) {
        strips[i] = new MyPixelBus(stripLengths[i], pins[i]);
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
    lastPacketTime = millis();
}

void loop() {
    if (Serial.available()) {
        byte receivedByte = Serial.read();
        switch (receivedByte) {
            case 'L': 
                { 
                    uint8_t buf[2];
                    if (Serial.readBytes(buf, 2) == 2) {
                        ReceivedSize = (buf[0] << 8) | buf[1];
                        if (ReceivedSize == 0) ReceivedSize = dofBlockSize; 
                    }
                } 
                break;
            case 'F': 
                if (!Fill()) { 
                    while(Serial.available()) Serial.read(); 
                }
                break;
            case 'R': 
                if (!ReceiveData()) { 
                    while(Serial.available()) Serial.read(); 
                }
                break;
            case 'O': 
                digitalWrite(FREQ_OUT_PIN, HIGH);
                canShow();
                ShowAll();
                UpdateFpsLed();
                digitalWrite(FREQ_OUT_PIN, LOW);
                break;
            case 'C': 
                for(uint8_t i = 0; i < NUM_STRIPS; i++) {
                    strips[i]->ClearTo(RgbColor(0));
                }
                ShowAll(); 
                break;
            case 'V': 
                while (!Serial);
                Serial.write(FirmwareVersionMajor); 
                Serial.write(FirmwareVersionMinor);
                break;       
            case 'M': 
                while (!Serial);
                Serial.write((byte)(dofBlockSize >> 8));
                Serial.write((byte)(dofBlockSize & 255));  
                break;  
            default: 
                while(Serial.available()) Serial.read(); 
                break;
        }
        Ack();
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
    vTaskDelay(1);
}

bool Fill() {
    uint8_t head[4];
    if (Serial.readBytes(head, 4) != 4) return false;
    uint16_t firstLed = (head[0] << 8) | head[1];
    uint16_t numberOfLeds = (head[2] << 8) | head[3];
    
    uint8_t rgb[3];
    if (Serial.readBytes(rgb, 3) != 3) return false;
    RgbColor color(rgb[0], rgb[1], rgb[2]);

    for (uint16_t i = 0; i < numberOfLeds; i++) { 
        uint16_t dofIndex = firstLed + i;
        uint16_t channel = dofIndex / ReceivedSize;
        uint16_t ledIndex = dofIndex % ReceivedSize;
        
        if (channel < NUM_STRIPS && ledIndex < stripLengths[channel]) {
            strips[channel]->SetPixelColor(ledIndex, color);
        }
    }
    return true;
}

bool ReceiveData() {
    uint8_t head[4];
    if (Serial.readBytes(head, 4) != 4) return false; 
    
    uint16_t firstLed     = (head[0] << 8) | head[1];
    uint16_t numberOfLeds = (head[2] << 8) | head[3];

    size_t bytesToRead = numberOfLeds * 3;
    if (bytesToRead > sizeof(rgbDataBuffer)) return false;
    if (Serial.readBytes(rgbDataBuffer, bytesToRead) != bytesToRead) return false;
 
    uint16_t dofIndex = firstLed;
    uint16_t channel  = dofIndex / ReceivedSize;
    uint16_t ledIndex = dofIndex % ReceivedSize;

    size_t bufIdx = 0;
    for (uint16_t i = 0; i < numberOfLeds; i++) {
        uint8_t r = rgbDataBuffer[bufIdx++];
        uint8_t g = rgbDataBuffer[bufIdx++];
        uint8_t b = rgbDataBuffer[bufIdx++];
       
        if (ledIndex +1 > stripLengths[channel]){channel++; ledIndex = firstLed;}

        if (channel < NUM_STRIPS && ledIndex < stripLengths[channel]) {
            strips[channel]->SetPixelColor(ledIndex, RgbColor(r, g, b));
        }
        ledIndex ++;
    }
    return true;
}