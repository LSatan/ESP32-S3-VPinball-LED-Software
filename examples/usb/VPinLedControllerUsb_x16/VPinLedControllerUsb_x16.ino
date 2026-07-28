#include <VPinLedControllerUsb.h>

#define FirmwareVersionMajor 3
#define FirmwareVersionMinor 1

#define NUM_STRIPS 16

// --- FEATURE SWITCH ---
bool enable_led_test = true; 

// --- LED power supply max mA power adapter ---
uint32_t maxCurrent_mA = 15000; // 15000 mA = 15A

// --- LED HARDWARE CONFIG ---
uint8_t fps_led_pin = 48;
uint8_t freq_out_pin = 2;

// --- AUTO-DETECT ARRAYS ---
uint16_t stripLengths[NUM_STRIPS] = {0}; 
uint16_t tempLengths[NUM_STRIPS] = {0};  

// --- Your HARDWARE PINS (ESP32-S3) ---
uint8_t pins[NUM_STRIPS] = {
    1, 4, 5, 6, 7, 15, 16, 17, 18,    // Channels 0 to 7
    8, 9, 10, 11, 12, 13, 14      // Channels 8 to 15
};

const uint16_t BufferSize = 24000;
uint16_t dofBlockSize = 1100;

// --- 16-CHANNEL ENGINE ---
typedef NeoPixelBus<NeoGrbFeature, NeoEsp32LcdX16Ws2812xMethod> MyPixelBus;
MyPixelBus* strips[NUM_STRIPS];

NeoPixelBus<NeoGrbFeature, NeoEsp32Rmt0Ws2812xMethod>* fpsLed = nullptr;
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
        if (strips[i] != nullptr){
            strips[i]->Show();
        }
    }
}

void canShow(){
    bool isReady = false;
    while (!isReady){
        isReady = true;
        for(uint8_t i = 0; i < NUM_STRIPS; i++) {
            if (strips[i] != nullptr){
                if (!strips[i]->CanShow()){
                    isReady = false;
                    break;
                }
            }
        }
    }
}

uint32_t totalCurrent_mA = 0;
void checkPowerLimit(){
    if (!maxCurrent_mA) {totalCurrent_mA = 0; return;}
    if (totalCurrent_mA > maxCurrent_mA) {
        float factor = (float)maxCurrent_mA / (float)totalCurrent_mA;
        uint8_t dimValue = (uint8_t)(factor * 255.0f);
        
        for (int channel = 0; channel < NUM_STRIPS; channel++) {
            if (strips[reverseIndex[channel]] != nullptr) {
                
                uint16_t ledCount = activeLengths[channel];
                
                for (uint16_t i = 0; i < ledCount; i++) {
                    RgbColor originalColor = strips[reverseIndex[channel]]->GetPixelColor(i);
                    strips[reverseIndex[channel]]->SetPixelColor(i, originalColor.Dim(dimValue));
                }
            }
        }
    }
    totalCurrent_mA = 0;
}

void UpdateFpsLed() {
    unsigned long currentMillis = millis();
    lastPacketTime = currentMillis;
    isStandby = false;

    if (fps_led_pin == 255){return;}
    frameCount++;
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

        fpsLed->SetPixelColor(0, applyBrightness(fpsColor, fpsLedBrightness));
        fpsLed->Show();
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
        if (pins[pinIndex[i]] != 255){
            strips[i] = new MyPixelBus(len, pins[pinIndex[i]]);
            strips[i]->Begin();
            strips[i]->ClearTo(RgbColor(0)); 
        }
    }

    ShowAll();
}

void loadSettings() {
    preferences.begin("vpin", false);
    
    enable_led_test = preferences.getBool("led", enable_led_test);
    fps_led_pin = preferences.getUChar("fps", fps_led_pin);
    freq_out_pin = preferences.getUChar("freq", freq_out_pin);
    maxCurrent_mA = preferences.getUInt("psuLimit", maxCurrent_mA);
    if (preferences.getBytesLength("pins") == NUM_STRIPS) {
        preferences.getBytes("pins", pins, NUM_STRIPS);
    }
    
    preferences.end(); 
}

void saveSettings() {
    preferences.begin("vpin", false);
    
    preferences.putBool("led", enable_led_test);
    preferences.putUChar("fps", fps_led_pin);
    preferences.putUChar("freq", freq_out_pin);
    preferences.putUInt("psuLimit", maxCurrent_mA);
    preferences.putBytes("pins", pins, NUM_STRIPS);
    
    preferences.end();
}

void setup() {
    Serial.setRxBufferSize(BufferSize);
    Serial.begin(2000000);
    Serial.setTimeout(100);
    Serial.setTxTimeoutMs(0);

    loadSettings();
    if (freq_out_pin != 255){pinMode(freq_out_pin, OUTPUT);}

    if (freq_out_pin != 255){
        fpsLed = new NeoPixelBus<NeoGrbFeature, NeoEsp32Rmt0Ws2812xMethod>(1, fps_led_pin);
        fpsLed->Begin();
    }

    for(uint8_t i = 0; i < NUM_STRIPS; i++) {
        if (pins[i] != 255){
            strips[i] = new MyPixelBus(dofBlockSize, pins[i]);
            strips[i]->Begin();
            strips[i]->ClearTo(RgbColor(0));
        }else{
            strips[i] = nullptr;
        } 
    }
    ShowAll();

    if(enable_led_test){
        uint32_t start = millis();
        while (millis() - start < 500) {
            if (Serial.available() > 0) return;
            yield();
        }

        if (fps_led_pin != 255 && fpsLed != nullptr){
            fpsLed->SetPixelColor(0, applyBrightness(RgbColor(255, 255, 255), fpsLedBrightness)); 
            fpsLed->Show();
        }

        for(uint8_t i=0; i<NUM_STRIPS; i++) {
            if (strips[i] != nullptr) strips[i]->ClearTo(RgbColor(255, 0, 0));
        }
        ShowAll(); delay(500);
        
        for(uint8_t i=0; i<NUM_STRIPS; i++) {
            if (strips[i] != nullptr) strips[i]->ClearTo(RgbColor(0, 255, 0));
        }
        ShowAll(); delay(500);
        
        for(uint8_t i=0; i<NUM_STRIPS; i++) {
            if (strips[i] != nullptr) strips[i]->ClearTo(RgbColor(0, 0, 255));
        }
        ShowAll(); delay(500);
        
        for(uint8_t i=0; i<NUM_STRIPS; i++) {
            if (strips[i] != nullptr) strips[i]->ClearTo(RgbColor(0, 0, 0));
        }
        ShowAll();

        if (fps_led_pin != 255 && fpsLed != nullptr){
            fpsLed->SetPixelColor(0, applyBrightness(RgbColor(0, 0, 0), fpsLedBrightness)); 
            fpsLed->Show();
        }
    }
    
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
                if (freq_out_pin != 255){digitalWrite(freq_out_pin, HIGH);}
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
                    checkPowerLimit();
                    canShow();
                    ShowAll();
                }
                resetStripes = false;
                UpdateFpsLed();
                if (freq_out_pin != 255){digitalWrite(freq_out_pin, LOW);}
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
                    if (freq_out_pin != 255){digitalWrite(freq_out_pin, HIGH);}
                
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
                        checkPowerLimit();
                        canShow();
                        ShowAll();
                    }
                    resetStripes = false;
                    UpdateFpsLed();
                    if (freq_out_pin != 255){digitalWrite(freq_out_pin, LOW);}
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
            case '?': {
                String cmd = Serial.readStringUntil('\n');
                cmd.trim();
                
                if (cmd == "INFO") {
                    String pinString = "";
                    for (int i = 0; i < 16; i++) {
                        if (pins[i] == 255) pinString += "None";
                        else pinString += String(pins[i]);
                        if (i < 15) pinString += ","; 
                    }

                    String fpsStr = (fps_led_pin == 255) ? "None" : String(fps_led_pin);
                    String freqStr = (freq_out_pin == 255) ? "None" : String(freq_out_pin);

                    String response = "S3_FW:USB;";
                    response += "LED:" + String(enable_led_test ? 1 : 0) + ";";
                    response += "PSU:" + String(maxCurrent_mA) + ";";
                    response += "FPS:" + fpsStr + ";";
                    response += "FREQ:" + freqStr + ";";
                    response += "PINS:" + pinString;
                    
                    Serial.println(response);
                }
            }
            case '!': {
                String payload = Serial.readStringUntil('\n');
                payload.trim();
                
                if (payload.startsWith("SAVE;")) {
                    String data = payload.substring(5); 
                    int startIndex = 0;

                    uint8_t tempPins[NUM_STRIPS];
                    for(int i = 0; i < NUM_STRIPS; i++) tempPins[i] = pins[i];
                    uint8_t tempFps = fps_led_pin;
                    uint8_t tempFreq = freq_out_pin;
                    bool tempLedTest = enable_led_test;

                    while (startIndex < data.length()) {
                        int scIndex = data.indexOf(';', startIndex);
                        String pair = (scIndex == -1) ? data.substring(startIndex) : data.substring(startIndex, scIndex);
                        startIndex = (scIndex == -1) ? data.length() : scIndex + 1;

                        int colon = pair.indexOf(':');
                        if (colon != -1) {
                            String key = pair.substring(0, colon);
                            String val = pair.substring(colon + 1);
                            
                            if (key == "LED") {
                                tempLedTest = (val == "1");
                            } 
                            else if (key == "FPS") {
                                tempFps = (uint8_t)val.toInt();
                            }
                            else if (key == "PSU") {
                                maxCurrent_mA = (uint32_t)val.toInt();
                            }
                            else if (key == "FREQ") {
                                tempFreq = (uint8_t)val.toInt();
                            } 
                            else if (key == "PINS") {
                                int pStart = 0;
                                int arrIndex = 0;
                                while (pStart < val.length() && arrIndex < 16) {
                                    int comma = val.indexOf(',', pStart);
                                    String p = (comma == -1) ? val.substring(pStart) : val.substring(pStart, comma);
                                    pStart = (comma == -1) ? val.length() : comma + 1;
                                    
                                    tempPins[arrIndex] = (uint8_t)p.toInt();
                                    arrIndex++;
                                }
                            }
                        }
                    }

                    bool outputsChanged = false;
                    for(int i=0; i<NUM_STRIPS; i++) {
                        if(pins[i] != tempPins[i]) outputsChanged = true;
                    }
                    bool freqChanged = (freq_out_pin != tempFreq);
                    bool fpsChanged = (fps_led_pin != tempFps);

                    if (outputsChanged) {
                        for(uint8_t i = 0; i < NUM_STRIPS; i++) {
                            if (strips[i] != nullptr) {
                                delete strips[i];
                                strips[i] = nullptr;
                            }
                        }
                        delay(20); 
                    }
                    if (fpsChanged) {
                        if (fpsLed != nullptr) {
                            delete fpsLed;
                            fpsLed = nullptr;
                        }
                        if(fps_led_pin != 255) {
                            pinMode(fps_led_pin, INPUT);
                        }
                    }

                    if (freqChanged) {
                        if(freq_out_pin != 255) {
                            pinMode(freq_out_pin, INPUT);
                        }
                    }

                    for(int i=0; i<NUM_STRIPS; i++) pins[i] = tempPins[i];
                    fps_led_pin = tempFps;
                    freq_out_pin = tempFreq;
                    enable_led_test = tempLedTest;

                    if (outputsChanged) {
                        ReconfigureLcdDma(stripLengths);
                    }
                     if (fpsChanged) {
                        if (fps_led_pin != 255){
                            fpsLed = new NeoPixelBus<NeoGrbFeature, NeoEsp32Rmt0Ws2812xMethod>(1, fps_led_pin);
                            fpsLed->Begin();
                        }
                    }

                    if (freqChanged) {
                        if(freq_out_pin != 255) {
                            pinMode(freq_out_pin, OUTPUT);
                            digitalWrite(freq_out_pin, LOW);
                        }
                    }

                    saveSettings();
                }
                else if (payload == "RESET") {
                    nvs_flash_erase(); 
                    nvs_flash_init();
                    delay(100);
                    ESP.restart();
                }
                break;
            }
            default: 
                while(Serial.available()) Serial.read(); 
                break;
        }
        Ack();
    }
    else if (!isStandby && (millis() - lastPacketTime > 1500)) {
        if (fps_led_pin != 255){
            fpsLed->SetPixelColor(0, RgbColor(0, 0, 0));
            fpsLed->Show();
        }
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
                if (strips[reverseIndex[channel]] != nullptr){
                    strips[reverseIndex[channel]]->SetPixelColor(ledIndex, RgbColor(rgb[0], rgb[1], rgb[2]));
                }
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
            if (strips[reverseIndex[channel]] != nullptr){
                strips[reverseIndex[channel]]->SetPixelColor(ledIndex, RgbColor(r, g, b));
                totalCurrent_mA += (((r + g + b) * 20) / 255) + 1;
            }
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
                if (strips[reverseIndex[channel]] != nullptr){
                    strips[reverseIndex[channel]]->SetPixelColor(ledIndex, RgbColor(r, g, b));
                    totalCurrent_mA += (((r + g + b) * 20) / 255) + 1;
                }
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
                    if (strips[reverseIndex[channel]] != nullptr){
                        strips[reverseIndex[channel]]->SetPixelColor(ledIndex, RgbColor(r, g, b));
                        totalCurrent_mA += (((r + g + b) * 20) / 255) + 1;
                    }
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
                if (strips[reverseIndex[channel]] != nullptr){
                    strips[reverseIndex[channel]]->SetPixelColor(ledIndex, RgbColor(r, g, b));
                    totalCurrent_mA += (((r + g + b) * 20) / 255) + 1;
                }
                ledIndex++;
            }
        }
    }
    return true;
}