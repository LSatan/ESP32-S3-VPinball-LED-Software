#include <VPinLedControllerWifi.h>
#define NUM_STRIPS 16

// --- FEATURE SWITCH ---
bool enable_led_test = true;

// --- LED power supply max mA power adapter ---
uint32_t maxCurrent_mA = 15000; // 15000 mA = 15A

// --- LED HARDWARE CONFIG ---
uint8_t fps_led_pin = 48;
uint8_t freq_out_pin = 2;

// --- Your HARDWARE PINS (ESP32-S3) ---
uint8_t pins[NUM_STRIPS] = {
    1, 4, 5, 6, 7, 15, 16, 17, 18,    // Channels 0 to 7
    8, 9, 10, 11, 12, 13, 14      // Channels 8 to 15
};


// --- WLAN ACCESS POINT DATA ---
String ap_ssid = "VPin_LED_Controller";       
String ap_pass = "vpinpassword";
String ssid = "YOUR SSID";       
String pass = "YOUR PASS";

// Define a fixed IP address for the AP
bool use_ap_mode = true;
bool use_dhcp = true;
IPAddress local_ip(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);

uint16_t port = 6454;
WiFiUDP udp;

// --- 16-CHANNEL ENGINE ---
typedef NeoPixelBus<NeoGrbFeature, NeoEsp32LcdX16Ws2812xMethod> MyPixelBus;
MyPixelBus* strips[NUM_STRIPS];

NeoPixelBus<NeoGrbFeature, NeoEsp32Rmt0Ws2812xMethod>* fpsLed = nullptr;
uint8_t fpsLedBrightness = 30;



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

    ssid = preferences.getString("ssid", ssid);
    pass = preferences.getString("pass", pass);
    ap_ssid = preferences.getString("ap_ssid", ap_ssid);
    ap_pass = preferences.getString("ap_pass", ap_pass);
    enable_led_test = preferences.getBool("led", enable_led_test);
    use_dhcp = preferences.getBool("dhcp", false);
    use_ap_mode = preferences.getBool("ap", true);
    fps_led_pin = preferences.getUChar("fps", fps_led_pin);
    freq_out_pin = preferences.getUChar("freq", freq_out_pin);
    maxCurrent_mA = preferences.getUInt("psuLimit", maxCurrent_mA);
    String savedIp = preferences.getString("ip", "192, 168, 4, 1");
    local_ip.fromString(savedIp);
    IPAddress local_ip(local_ip);
    port = preferences.getUShort("port", 6454);
    if (preferences.getBytesLength("pins") == NUM_STRIPS) {
        preferences.getBytes("pins", pins, NUM_STRIPS);
    }
    
    preferences.end(); 
}

void saveSettings() {
    preferences.begin("vpin", false);
    
    preferences.putString("ssid", ssid);
    preferences.putString("pass", pass);
    preferences.putString("ap_ssid", ap_ssid);
    preferences.putString("ap_pass", ap_pass);
    preferences.putBool("led", enable_led_test);
    preferences.putBool("dhcp", use_dhcp);
    preferences.putBool("ap", use_ap_mode);
    preferences.putUChar("fps", fps_led_pin);
    preferences.putUChar("freq", freq_out_pin);
    preferences.putUInt("psuLimit", maxCurrent_mA);
    preferences.putString("ip", local_ip.toString());
    preferences.putUShort("port", port);
    preferences.putBytes("pins", pins, NUM_STRIPS);

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

    WiFi.disconnect(true, true);
    delay(100);

    if (use_ap_mode) {

        Serial.println("\nStarting Access Point mode...");
        WiFi.mode(WIFI_AP);
        
        WiFi.softAPConfig(local_ip, local_ip, subnet);
        WiFi.softAP(ap_ssid.c_str(), ap_pass.c_str());

        Serial.print("Hotspot '"); Serial.print(ap_ssid); Serial.println("' is active!");
        Serial.print("IP: "); Serial.println(WiFi.softAPIP());
    } 
    else {
        Serial.println("\nConnecting to Router (STA mode)...");
        WiFi.mode(WIFI_STA);

        if (!use_dhcp) {

            IPAddress gateway = local_ip;
            gateway[3] = 1;
            WiFi.config(local_ip, gateway, subnet);
        }

        if (ssid.length() > 0) {
            WiFi.begin(ssid.c_str(), pass.c_str());
            
            uint32_t startAttempt = millis();
            while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 5000) {
                delay(100);
            }
        }
        
        if (WiFi.status() == WL_CONNECTED) {
            Serial.print("Connected! IP: "); Serial.println(WiFi.localIP());
        } else {
            Serial.println("Connection pending... (running in background)");
        }
    }

    udp.begin(port);
    lastPacketTime = millis();
}

void loop() {

    if (Serial.available()) {
        byte receivedByte = Serial.read();
        if(receivedByte == '?'){
            String cmd = Serial.readStringUntil('\n');
            cmd.trim();

			if (cmd == "INFO") {
				String pinString = "";
				for(int i=0; i<NUM_STRIPS; i++) {
					pinString += String(pins[i]);
					if(i < NUM_STRIPS-1) pinString += ",";
				}

				String response = "S3_FW:WIFI;";
				response += "LED:" + String(enable_led_test ? 1 : 0) + ";"; 
				response += "FPS:" + String(fps_led_pin) + ";";
				response += "FREQ:" + String(freq_out_pin) + ";";
                response += "PSU:" + String(maxCurrent_mA) + ";";
				response += "PINS:" + pinString + ";";
				response += "AP:" + String(use_ap_mode ? "1" : "0") + ";";
				response += "DHCP:" + String(use_dhcp ? "1" : "0") + ";";
				response += "APS:" + String(ap_ssid) + ";";
				response += "APP:" + String(ap_pass) + ";";
				response += "SSID:" + ssid + ";";
				response += "PASS:" + pass + ";";
				
				if (use_ap_mode) {
					response += "IP:" + WiFi.softAPIP().toString() + ";";
				} else {
					response += "IP:" + WiFi.localIP().toString() + ";";
				}
				
				response += "PORT:" + String(port);

				Serial.println(response);
			}
        }
        else if(receivedByte == '!'){
            String payload = Serial.readStringUntil('\n');
            payload.trim();
			if (payload.startsWith("SAVE;")) {
				String data = payload.substring(5); 
				int startIndex = 0;
				
				uint8_t tempPins[NUM_STRIPS];
				for(int i = 0; i < NUM_STRIPS; i++) tempPins[i] = pins[i];
				uint8_t tempFps = fps_led_pin;
				uint8_t tempFreq = freq_out_pin;
				bool tempAp = use_ap_mode;
				bool tempDhcp = use_dhcp;
				String tempSsid = ssid;
				String tempPass = pass;
				String tempAps = ap_ssid;
				String tempApp = ap_pass;
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
						
						if (key == "AP") tempAp = (val == "1");
						else if (key == "DHCP") tempDhcp = (val == "1");
						else if (key == "SSID") tempSsid = val;
						else if (key == "PASS") tempPass = val;
						else if (key == "APS") tempAps = val;
						else if (key == "APP") tempApp = val;
						else if (key == "IP") tempIp.fromString(val);
						else if (key == "PORT") tempPort = (uint16_t)val.toInt();
						else if (key == "LED") {tempLedTest = (val == "1");} 
						else if (key == "FPS") {tempFps = (uint8_t)val.toInt();}
                        else if (key == "PSU") {maxCurrent_mA = (uint32_t)val.toInt();}
						else if (key == "FREQ") {tempFreq = (uint8_t)val.toInt();} 
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
					}
				}

				bool outputsChanged = false;
				for(int i=0; i<NUM_STRIPS; i++) {
					if(pins[i] != tempPins[i]) outputsChanged = true;
				}
				bool freqChanged = (freq_out_pin != tempFreq);
				bool fpsChanged = (fps_led_pin != tempFps);
				bool networkChanged = (use_ap_mode != tempAp || use_dhcp != tempDhcp || ap_ssid != tempAps || ap_pass != tempApp || ssid != tempSsid || pass != tempPass || local_ip != tempIp || port != tempPort);

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

				for(int i=0; i<NUM_STRIPS; i++) pins[i] = tempPins[i];
				fps_led_pin = tempFps;
				freq_out_pin = tempFreq;
				enable_led_test = tempLedTest;
				use_ap_mode = tempAp;
				use_dhcp = tempDhcp;
				ap_ssid = tempAps;
				ap_pass = tempApp;
				ssid = tempSsid;
				pass = tempPass;
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

				saveSettings();
                
                while(!Serial);
				Serial.write('A');

				if (networkChanged) {
					resetMagicNumber = 12345678;
					delay(500);
					ESP.restart();
				}
			}
			else if (payload == "RESET") {
				nvs_flash_erase(); 
				nvs_flash_init();
				
                while(!Serial);
				Serial.write('A'); 
				
				delay(100);
				ESP.restart();
			}
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
            
                if (stripLengths[i] != activeLengths[i]) {
                    layoutChanged = true;
                }
            
            }

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
                            totalCurrent_mA += (((r + g + b) * 20) / 255) + 1;
                        }
                    }
                }
            }
            if (freq_out_pin != 255) digitalWrite(freq_out_pin, LOW);
            checkPowerLimit();
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
}