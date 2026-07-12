#include <VPinLedControllerUsb.h>

#define FirmwareVersionMajor 3
#define FirmwareVersionMinor 1

// --- FEATURE SWITCH ---
#define ENABLE_LED_TEST 1 

// --- LED HARDWARE CONFIG ---
#define FPS_LED_PIN 48
#define FREQ_OUT_PIN 2
#define NUM_STRIPS 16

// --- AUTO-DETECT ARRAYS ---
uint16_t stripLengths[NUM_STRIPS] = {0}; 
uint16_t tempLengths[NUM_STRIPS] = {0};  

// --- Your HARDWARE PINS (ESP32-S3) ---
const uint8_t pins[NUM_STRIPS] = {
    1, 4, 5, 6, 7, 15, 16, 17, 18,    // Channels 0 to 7
    8, 9, 10, 11, 12, 13, 14      // Channels 8 to 15
};

const uint16_t BufferSize = 16000;
uint16_t dofBlockSize = 1100;

// --- 16-CHANNEL ENGINE ---
typedef NeoPixelBus<NeoGrbFeature, NeoEsp32LcdX16Ws2812xMethod> MyPixelBus;
MyPixelBus* strips[NUM_STRIPS];

NeoPixelBus<NeoGrbFeature, NeoEsp32Rmt0Ws2812xMethod> fpsLed(1, FPS_LED_PIN);
uint8_t fpsLedBrightness = 30;

uint8_t rgbDataBuffer[BufferSize]; 
uint16_t ReceivedSize = dofBlockSize; 
uint16_t activeLengths[NUM_STRIPS] = {0};
uint8_t pinIndex[NUM_STRIPS];
uint8_t reverseIndex[NUM_STRIPS];

unsigned long lastPacketTime = 0;
bool isStandby = false;
bool resetStripes = false;
unsigned long lastFpsTime = 0;
int frameCount = 0;

RgbColor applyBrightness(RgbColor color, uint8_t brightness) {
    return RgbColor((color.R * brightness) / 255, (color.G * brightness) / 255, (color.B * brightness) / 255);
}

void Ack() {
    while (!Serial);
    uint8_t ackRetries = 0;
    while (Serial.write('A') == 0 && ackRetries < 50) {
        delayMicroseconds(100);
        yield();
        ackRetries++;
    }
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
    unsigned long currentMillis = millis();
    lastPacketTime = currentMillis;
    frameCount++;
    isStandby = false;

    if (currentMillis - lastFpsTime >= 1000) {
        int currentFps = frameCount;
        frameCount = 0;
        lastFpsTime = currentMillis;
        RgbColor fpsColor;

        if (currentFps < 20) fpsColor = RgbColor(255, 0, 0);       
        else if (currentFps <= 29) fpsColor = RgbColor(255, 100, 0); 
        else if (currentFps <= 39) fpsColor = RgbColor(255, 255, 0); 
        else if (currentFps <= 49) fpsColor = RgbColor(100, 255, 0); 
        else if (currentFps <= 59) fpsColor = RgbColor(0, 255, 0);   
        else if (currentFps <= 69) fpsColor = RgbColor(0, 255, 255); 
        else if (currentFps <= 89) fpsColor = RgbColor(0, 0, 255);   
        else if (currentFps <= 119) fpsColor = RgbColor(148, 0, 211);
        else fpsColor = RgbColor(255, 255, 255); 

        fpsLed.SetPixelColor(0, applyBrightness(fpsColor, fpsLedBrightness));
        fpsLed.Show();
    }
}

void ReconfigureLcdDma(uint16_t* newLengths) {

    for(uint8_t i = 0; i < NUM_STRIPS; i++) {
        if (strips[i] != nullptr) {
            delete strips[i];
            strips[i] = nullptr;
        }
    }
    delay(20); 

    for (int i = 0; i < NUM_STRIPS; i++) {
        pinIndex[i] = i;
        uint16_t exactLen = newLengths[i];
        uint16_t safeLen = (exactLen > 0) ? exactLen : 0;
        activeLengths[i] = safeLen;
    }

    std::sort(pinIndex, pinIndex + NUM_STRIPS, [](int a, int b) {
        return activeLengths[a] > activeLengths[b];
    });
    
    for(uint8_t k = 0; k < NUM_STRIPS; k++) {
        reverseIndex[pinIndex[k]] = k; 
    }
    
    for(uint8_t i = 0; i < NUM_STRIPS; i++) {
        uint16_t len = activeLengths[pinIndex[i]];
        if (len == 0) len = 1; 
        strips[i] = new MyPixelBus(len, pins[pinIndex[i]]);
        strips[i]->Begin();
        strips[i]->ClearTo(RgbColor(0)); 
    }

    ShowAll();
}

void setup() {
    Serial.setRxBufferSize(BufferSize);
    Serial.begin(2000000);
    pinMode(FREQ_OUT_PIN, OUTPUT);
    Serial.setTimeout(100);
    Serial.setTxTimeoutMs(0);

    for(uint8_t i = 0; i < NUM_STRIPS; i++) {
        strips[i] = new MyPixelBus(dofBlockSize, pins[i]);
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
            case 'Z': 
                {
                    if (!resetStripes){
                        for(uint8_t i = 0; i < NUM_STRIPS; i++) {
                            tempLengths[i] = 0; 
                        }
                        resetStripes = true;
                    }
                    uint8_t buf[4];
                    if (Serial.readBytes(buf, 4) == 4) {
                        uint8_t indexStrip = buf[0];
                        uint16_t stripLen = (buf[2] << 8) | buf[3];
                        if (indexStrip < NUM_STRIPS) {
                            tempLengths[indexStrip] = stripLen; 
                        }
                    }
                }
                break;
            case 'W':
                {
                if (!ReceiveBulkData()) {
                    while(Serial.available()) Serial.read(); 
                }
                digitalWrite(FREQ_OUT_PIN, HIGH);
                bool layoutChanged = false;
                for (int i = 0; i < NUM_STRIPS; i++) {
                    if (tempLengths[i] != stripLengths[i]) {
                        layoutChanged = true;
                        stripLengths[i] = tempLengths[i];
                    }
                }
                if (layoutChanged) {
                    ReconfigureLcdDma(stripLengths);
                } else {
                    canShow();
                    ShowAll();
                }
                resetStripes = false;
                UpdateFpsLed();
                digitalWrite(FREQ_OUT_PIN, LOW);
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
            case 'Q': 
                if (!ReceiveCompressedData()) {
                    while(Serial.available()) Serial.read(); 
                }
                break;
            case 'O': 
                {
                    digitalWrite(FREQ_OUT_PIN, HIGH);
                    
                    bool layoutChanged = false;
                    for (int i = 0; i < NUM_STRIPS; i++) {
                        if (tempLengths[i] != stripLengths[i]) {
                            layoutChanged = true;
                            stripLengths[i] = tempLengths[i];
                        }
                    }

                    if (layoutChanged) {
                        ReconfigureLcdDma(stripLengths);
                    } else {
                        canShow();
                        ShowAll();
                    }
                    resetStripes = false;
                    UpdateFpsLed();
                    digitalWrite(FREQ_OUT_PIN, LOW);
                }
                break;
            case 'C': 
                for(uint8_t i = 0; i < NUM_STRIPS; i++) {
                    if (strips[i] != nullptr) strips[i]->ClearTo(RgbColor(0));
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
    
    uint16_t dofIndex = firstLed;
    uint16_t channel = dofIndex / ReceivedSize;
    uint16_t ledIndex = dofIndex % ReceivedSize;

    if (numberOfLeds != tempLengths[channel]) tempLengths[channel] = numberOfLeds;
    for (uint16_t i = 0; i < numberOfLeds; i++) { 

        if (channel < NUM_STRIPS) {
            if (ledIndex < stripLengths[channel]) {
                strips[reverseIndex[channel]]->SetPixelColor(ledIndex, RgbColor(rgb[0], rgb[1], rgb[2]));
            }
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

    if (numberOfLeds != tempLengths[channel]) tempLengths[channel] = numberOfLeds;
    for (uint16_t i = 0; i < numberOfLeds; i++) {
        uint8_t r = rgbDataBuffer[bufIdx++];
        uint8_t g = rgbDataBuffer[bufIdx++];
        uint8_t b = rgbDataBuffer[bufIdx++];
       
        if (channel < NUM_STRIPS && ledIndex < stripLengths[channel]) {
            strips[reverseIndex[channel]]->SetPixelColor(ledIndex, RgbColor(r, g, b));
        }
        ledIndex ++;
    }
    return true;
}

bool ReceiveCompressedData() {
    uint8_t head[6];
    if (Serial.readBytes(head, 6) != 6) return false; 
    
    uint16_t firstLed = (head[0] << 8) | head[1];
    uint16_t numCompressedData = (head[2] << 8) | head[3];

	// Each compressed packet consists of 4 bytes (1 byte length + 3 bytes RGB)
    size_t bytesToRead = numCompressedData * 4;
    if (bytesToRead > sizeof(rgbDataBuffer)) return false;
    if (Serial.readBytes(rgbDataBuffer, bytesToRead) != bytesToRead) return false;

    uint16_t dofIndex = firstLed;
    uint16_t channel = dofIndex / ReceivedSize;
    uint16_t ledIndex = dofIndex % ReceivedSize;
    size_t bufIdx = 0;

    for (uint16_t pack = 0; pack < numCompressedData; pack++) {
        uint8_t nbLeds = rgbDataBuffer[bufIdx++];
        uint8_t r = rgbDataBuffer[bufIdx++];
        uint8_t g = rgbDataBuffer[bufIdx++];
        uint8_t b = rgbDataBuffer[bufIdx++];
  
        for (uint8_t numLed = 0; numLed < nbLeds; numLed++) {

            if (channel < NUM_STRIPS && ledIndex < stripLengths[channel]) {
                strips[reverseIndex[channel]]->SetPixelColor(ledIndex, RgbColor(r, g, b));
            }
            ledIndex++;
        }
    }
    if (ledIndex != tempLengths[channel]) tempLengths[channel] = ledIndex;
    return true;
}


bool ReceiveBulkData() {
    uint8_t head[3];
    if (Serial.readBytes(head, 3) != 3) return false; 
    
    uint8_t useCompression = head[0];
    uint16_t packetSize = (head[1] << 8) | head[2];

    size_t bytesToRead = packetSize * (3 + useCompression);
    if (bytesToRead > sizeof(rgbDataBuffer)) return false;
    if (Serial.readBytes(rgbDataBuffer, bytesToRead) != bytesToRead) return false;

    uint16_t channel = 0;
    uint16_t ledIndex = 0;
    size_t bufIdx = 0;

    if (useCompression) {
        for (uint16_t pack = 0; pack < packetSize; pack++) {
            uint8_t nbLeds = rgbDataBuffer[bufIdx++];
            uint8_t r = rgbDataBuffer[bufIdx++];
            uint8_t g = rgbDataBuffer[bufIdx++];
            uint8_t b = rgbDataBuffer[bufIdx++];
    
            for (uint8_t numLed = 0; numLed < nbLeds; numLed++) {

                while (channel < NUM_STRIPS && ledIndex >= stripLengths[channel]) {
                    channel++;
                    ledIndex = 0;
                }

                if (channel < NUM_STRIPS) {
                    strips[reverseIndex[channel]]->SetPixelColor(ledIndex, RgbColor(r, g, b));
                    ledIndex++;
                }
            }
        }
    } else {
        for (uint16_t i = 0; i < packetSize; i++) {
            uint8_t r = rgbDataBuffer[bufIdx++];
            uint8_t g = rgbDataBuffer[bufIdx++];
            uint8_t b = rgbDataBuffer[bufIdx++];
        
            while (channel < NUM_STRIPS && ledIndex >= stripLengths[channel]) {
                channel++;
                ledIndex = 0;
            }

            if (channel < NUM_STRIPS) {
                strips[reverseIndex[channel]]->SetPixelColor(ledIndex, RgbColor(r, g, b));
                ledIndex++;
            }
        }
    }
    return true;
}




