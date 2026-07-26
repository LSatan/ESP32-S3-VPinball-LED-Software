#include <VPinLedControllerEthernet.h>
#define NUM_STRIPS 16

// --- FEATURE SWITCH ---
bool enable_led_test = true;

// --- W5500 LAN PINS ---
#define W5500_MOSI 35
#define W5500_SCK  36
#define W5500_MISO 37
#define W5500_CS   39
#define W5500_RST  41
// Index-Mapping: 0=MOSI, 1=MISO, 2=SCLK, 3=CS, 4=RST
uint8_t lan_pins[5] = {35, 37, 36, 39, 41};

// The INT pin is not needed by the Ethernet library,
// it simply remains unused.

// --- LED HARDWARE CONFIG ---
uint8_t fps_led_pin = 48;
uint8_t freq_out_pin = 2;

// --- Your HARDWARE PINS (ESP32-S3) ---
uint8_t pins[NUM_STRIPS] = {
    1, 4, 5, 6, 7, 15, 16, 17, 18,    // Channels 0 to 7
    8, 9, 10, 11, 12, 13, 14      // Channels 8 to 15
};

// --- NETWORK CONFIG ---
// Since the W5500 handles the network stack itself, it needs a MAC address:
uint8_t mac[6];
IPAddress local_ip(192, 168, 1, 100); 

typedef NeoPixelBus<NeoGrbFeature, NeoEsp32LcdX16Ws2812xMethod> MyPixelBus;
MyPixelBus* strips[NUM_STRIPS];

NeoPixelBus<NeoGrbFeature, NeoEsp32Rmt0Ws2812xMethod>* fpsLed = nullptr;
uint8_t fpsLedBrightness = 30;

// Here we are now using the dedicated Ethernet UDP class
EthernetUDP udp; 
uint16_t port = 6454;

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
    //use_dhcp = preferences.getBool("dhcp", false);
    // KORREKTUR: UChar (Unsigned Char) für uint8_t verwenden!
    fps_led_pin = preferences.getUChar("fps", fps_led_pin);
    freq_out_pin = preferences.getUChar("freq", freq_out_pin);
    
    String savedIp = preferences.getString("ip", "10.10.10.100");
    local_ip.fromString(savedIp);
    IPAddress local_ip(local_ip);
    port = preferences.getUShort("port", 6454);
    if (preferences.getBytesLength("pins") == NUM_STRIPS) {
        preferences.getBytes("pins", pins, NUM_STRIPS);
    }
    if (preferences.getBytesLength("lan_pins") == 5) {
        preferences.getBytes("lan_pins", lan_pins, 5);
    }
    
    preferences.end(); 
}

void saveSettings() {
    preferences.begin("vpin", false);
    
    preferences.putBool("led", enable_led_test);
    preferences.putUChar("fps", fps_led_pin);
    preferences.putUChar("freq", freq_out_pin);
    preferences.putString("ip", local_ip.toString());
    preferences.putUShort("port", port);
    preferences.putBytes("pins", pins, NUM_STRIPS);
    preferences.putBytes("lan_pins", lan_pins, 5);

    preferences.end();
}

void setup() {
    Serial.begin(115200);
	Serial0.begin(115200);
    loadSettings();

    if (freq_out_pin != 255){pinMode(freq_out_pin, OUTPUT);}

    if (freq_out_pin != 255){
        fpsLed = new NeoPixelBus<NeoGrbFeature, NeoEsp32Rmt0Ws2812xMethod>(1, fps_led_pin);
        fpsLed->Begin();
    }

    for(uint8_t i = 0; i < NUM_STRIPS; i++) {
        if (pins[i] != 255){
            strips[i] = new MyPixelBus(1100, pins[i]);
            strips[i]->Begin();
            strips[i]->ClearTo(RgbColor(0));
        }else{
            strips[i] = nullptr;
        } 
    }
    ShowAll();

    bool isSoftReset = (resetMagicNumber == 12345678);
    resetMagicNumber = 0;

    if(enable_led_test && !isSoftReset){
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
    
    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    if (lan_pins[0] != 255 && lan_pins[1] != 255 && lan_pins[2] != 255 && lan_pins[3] != 255 && lan_pins[4] != 255) {
        pinMode(lan_pins[4], OUTPUT);
        digitalWrite(lan_pins[4], LOW);
        delay(10);
        digitalWrite(lan_pins[4], HIGH);
        delay(150);
        SPI.begin(lan_pins[2], lan_pins[1], lan_pins[0], -1);
        Ethernet.init(lan_pins[3]);
        Ethernet.begin(mac, local_ip);
        //Serial.println("Warte auf Netzwerk-Link...");
        while (Ethernet.linkStatus() != LinkON) {
            delay(100);
        }
        //Serial.println("Network connected!");
        //delay(100);
        udp.begin(port);
        delay(100);
        udp.flush();
    }
    lastPacketTime = millis();
}


void loop() {

    if (Serial.available()) {
        activeSerial = &Serial;   // Nativ USB-Port
    } 
    else if (Serial0.available()) {
        activeSerial = &Serial0;  // UART-Port
    }

    if (activeSerial->available()) {
        byte receivedByte = activeSerial->read();
        if(receivedByte == '?'){
                String cmd = activeSerial->readStringUntil('\n');
                cmd.trim();
                
                if (cmd == "INFO") {

                    String pinString = "";
                    for (int i = 0; i < 16; i++) {
                        if (pins[i] == 255) pinString += "None";
                        else pinString += String(pins[i]);
                        if (i < 15) pinString += ","; 
                    }
                    String lanPinStr = "";
                    for (int i = 0; i < 5; i++) {
                        lanPinStr += String(lan_pins[i]);
                        if (i < 4) lanPinStr += ",";
                    }

                    String fpsStr = (fps_led_pin == 255) ? "None" : String(fps_led_pin);
                    String freqStr = (freq_out_pin == 255) ? "None" : String(freq_out_pin);
                    String portStr = String(port);
                    String response = "S3_FW:LAN;";
                    response += "LED:" + String(enable_led_test ? 1 : 0) + ";";
                    response += "FPS:" + fpsStr + ";";
                    response += "FREQ:" + freqStr + ";";
                    response += "PINS:" + pinString + ";";
                    response += "LANPINS:" + lanPinStr + ";";
                    response += "IP:" + Ethernet.localIP().toString() + ";";
                    response += "PORT:" + portStr;

                    activeSerial->println(response);
                }
        }
        else if(receivedByte == '!'){
                String payload = activeSerial->readStringUntil('\n');
                payload.trim();
                
                if (payload.startsWith("SAVE;")) {
                    String data = payload.substring(5); 
                    int startIndex = 0;
                    uint8_t tempPins[NUM_STRIPS];
                    uint8_t tempLanPins[5];
                    for(int i=0; i<5; i++) tempLanPins[i] = lan_pins[i];
                    for(int i = 0; i < NUM_STRIPS; i++) tempPins[i] = pins[i];
                    uint8_t tempFps = fps_led_pin;
                    uint8_t tempFreq = freq_out_pin;
                    IPAddress tempIp = local_ip;
                    uint16_t tempPort = port;
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
                            else if (key == "FREQ") {
                                tempFreq = (uint8_t)val.toInt();
                            } 
                            else if (key == "PINS") {
                                int pStart = 0;
                                int arrIndex = 0;
                                while (pStart < val.length() && arrIndex < NUM_STRIPS) {
                                    int comma = val.indexOf(',', pStart);
                                    String p = (comma == -1) ? val.substring(pStart) : val.substring(pStart, comma);
                                    pStart = (comma == -1) ? val.length() : comma + 1;
                                    
                                    tempPins[arrIndex] = (uint8_t)p.toInt();
                                    arrIndex++;
                                }
                            }
                            else if (key == "LANPINS") {
                                int pStart = 0;
                                int arrIndex = 0;
                                while (pStart < val.length() && arrIndex < 5) {
                                    int comma = val.indexOf(',', pStart);
                                    String p = (comma == -1) ? val.substring(pStart) : val.substring(pStart, comma);
                                    pStart = (comma == -1) ? val.length() : comma + 1;
                                    
                                    tempLanPins[arrIndex] = (uint8_t)p.toInt();
                                    arrIndex++;
                                }
                            }
                            else if (key == "IP") {
                                tempIp.fromString(val);
                            }
                            else if (key == "PORT") {
                                tempPort = (uint16_t)val.toInt();
                            }
                        }
                    }

                    bool outputsChanged = false;
                    for(int i=0; i<NUM_STRIPS; i++) {
                        if(pins[i] != tempPins[i]) outputsChanged = true;
                    }
                    bool lanChanged = false;
                    for(int i=0; i<5; i++) {
                        if(lan_pins[i] != tempLanPins[i]) lanChanged = true;
                    }
                    bool freqChanged = (freq_out_pin != tempFreq);
                    bool fpsChanged = (fps_led_pin != tempFps);
                    bool ipChanged = (local_ip != tempIp);
                    bool portChanged = (port != tempPort);

                    if (portChanged || ipChanged) {
                        udp.stop();
                        delay(10);        
                    }

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
                        if(fps_led_pin != 255) pinMode(fps_led_pin, INPUT);
                    }

                    if (freqChanged) {
                        if(freq_out_pin != 255) pinMode(freq_out_pin, INPUT);
                    }

                    if (lanChanged) {
                        pinMode(lan_pins[4], INPUT);
                    }

                    for(int i=0; i<NUM_STRIPS; i++) pins[i] = tempPins[i];
                    for(int i=0; i<5; i++) lan_pins[i] = tempLanPins[i];
                    fps_led_pin = tempFps;
                    freq_out_pin = tempFreq;
                    enable_led_test = tempLedTest;
                    local_ip = tempIp;
                    port = tempPort;

                    if (outputsChanged) {
                        ReconfigureLcdDma(activeLengths);
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
      
                    if (ipChanged) {
                        if (lan_pins[0] != 255 && lan_pins[1] != 255 && lan_pins[2] != 255 && lan_pins[3] != 255) {
                            Ethernet.begin(mac, local_ip);
                            delay(10);
                        }
                    }

                    if (portChanged || ipChanged) {
                        udp.begin(port);
                    }

                    saveSettings();
                    
                    while(!activeSerial);
                    activeSerial->write('A');
                    
                    if (lanChanged) {
                        resetMagicNumber = 12345678;
                        delay(500); 
                        ESP.restart();
                    }
                }
                else if (payload == "RESET") {
                    nvs_flash_erase(); 
                    nvs_flash_init();
                    
                    activeSerial->write('A'); 
                    
                    delay(100);
                    ESP.restart();
                }
        }else{
            while(activeSerial->available()) activeSerial->read();
        }
    }

    int packetSize = udp.parsePacket();
    if (packetSize > 3) { 
        if (freq_out_pin != 255) digitalWrite(freq_out_pin, HIGH);
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
            
            // As soon as even one channel deviates, we know: New layout!
                if (stripLengths[i] != activeLengths[i]) {
                    layoutChanged = true;
                }
            
            }

            // 2. If it is a new layout -> trigger hardware reset!
            if (layoutChanged) {
                ReconfigureLcdDma(stripLengths);
            }

            int byteIndex = 33;
            for (int stripe = 0; stripe < NUM_STRIPS; stripe++) {
                uint16_t currentLen = stripLengths[stripe];
                if (currentLen > 0) {
					uint8_t channel = reverseIndex[stripe];
                    for (int len = 0; len < currentLen; len++) {
                        uint8_t r = frameBuffer[byteIndex++];
                        uint8_t g = frameBuffer[byteIndex++];
                        uint8_t b = frameBuffer[byteIndex++];
                        if (strips[channel] != nullptr){
                            strips[channel]->SetPixelColor(len, RgbColor(r, g, b));
                        }
                    }
                }
            }
            if (freq_out_pin != 255) digitalWrite(freq_out_pin, LOW);
            canShow();
            ShowAll();            
            UpdateFpsLed();
        }
    } 
    else if (!isStandby && (millis() - lastPacketTime > 1500)) {
        for(uint8_t i = 0; i < NUM_STRIPS; i++) {
            if (strips[i] != nullptr){
                strips[i]->ClearTo(RgbColor(0));
            }
        }
        ShowAll();
        
        if (fps_led_pin != 255){
            fpsLed->SetPixelColor(0, RgbColor(0, 0, 0));
            fpsLed->Show();
        }
        frameCount = 0; 
        
        isStandby = true; 
    }
    else if (Ethernet.hardwareStatus() == EthernetNoHardware) { 
        resetMagicNumber = 12345678;
        ESP.restart();
    }
}