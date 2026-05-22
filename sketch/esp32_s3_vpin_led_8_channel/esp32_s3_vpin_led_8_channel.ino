#include <NeoPixelBus.h>

#define ChannelCount 8
#define FirmwareVersionMajor 1
#define FirmwareVersionMinor 3

// --- FEATURE SWITCH ---
// Set to 1 to have all LEDs light up in red, green, blue one after the other when turned on.
// Set to 0 to disable the test (cabinet remains dark at boot).
#define ENABLE_LED_TEST 1 

// --- YOUR PHYSICAL HARDWARE ---
const uint16_t stripLengths[ChannelCount] = { 144, 144, 144, 144, 144, 144, 144, 144 };
const uint8_t pins[ChannelCount] = { 4, 5, 6, 7, 8, 9, 10, 11 };

// Dynamic block size (Will be automatically overwritten by the 'L' command of DOF)
uint16_t dofBlockSize = 1100; 

typedef NeoPixelBus<NeoGrbFeature, NeoEsp32LcdX8Ws2812xMethod> MyPixelBus;
MyPixelBus* strips[ChannelCount];

uint8_t rgbDataBuffer[8192]; 

void Ack()  { Serial.write('A'); Serial.flush(); }
void Nack() { Serial.write('N'); Serial.flush(); }

void ShowAll() {
    for(uint8_t i = 0; i < ChannelCount; i++) {
        while(!strips[i]->CanShow()) { yield(); }
        strips[i]->Show();
    }
}

void setup() {
    Serial.setRxBufferSize(8192); 
    Serial.begin(115200);
    Serial.setTimeout(100); 
    
    // 1. Initialize hardware
    for(uint8_t i = 0; i < ChannelCount; i++) {
        strips[i] = new MyPixelBus(stripLengths[i], pins[i]);
        strips[i]->Begin();
        strips[i]->ClearTo(RgbColor(0));
    }
    ShowAll();
    
    // 2. Optional LED boot test
    #if ENABLE_LED_TEST
        // Rot
        for(uint8_t i=0; i<ChannelCount; i++) strips[i]->ClearTo(RgbColor(255, 0, 0));
        ShowAll();
        delay(500);
        
        // Grün
        for(uint8_t i=0; i<ChannelCount; i++) strips[i]->ClearTo(RgbColor(0, 255, 0));
        ShowAll();
        delay(500);
        
        // Blau
        for(uint8_t i=0; i<ChannelCount; i++) strips[i]->ClearTo(RgbColor(0, 0, 255));
        ShowAll();
        delay(500);
        
        // White (optional, but it draws a lot of power!)
        // for(uint8_t i=0; i<ChannelCount; i++) strips[i]->ClearTo(RgbColor(255, 255, 255));
        // ShowAll();
        // delay(500);

        // Turn everything off again and get ready for DOF
        for(uint8_t i=0; i<ChannelCount; i++) strips[i]->ClearTo(RgbColor(0, 0, 0));
        ShowAll();
    #endif
}

void loop() {
    if (Serial.available()) {
        byte receivedByte = Serial.read();
        switch (receivedByte) {
            case 'L': 
                { 
                    uint8_t buf[2]; 
                    if (Serial.readBytes(buf, 2) == 2) {
                        dofBlockSize = (buf[0] << 8) | buf[1];
                        if (dofBlockSize == 0) dofBlockSize = 1100; 
                    }
                    Ack(); 
                } 
                break;
            case 'F': 
                if (!Fill()) { Nack(); } 
                break;
            case 'R': 
                if (!ReceiveData()) { Nack(); } 
                break; 
            case 'O': 
                ShowAll(); 
                Ack(); 
                break;
            case 'C': 
                for(uint8_t i = 0; i < ChannelCount; i++) {
                    strips[i]->ClearTo(RgbColor(0)); 
                }
                ShowAll(); 
                Ack(); 
                break;
            case 'V': 
                Serial.write(FirmwareVersionMajor); 
                Serial.write(FirmwareVersionMinor); 
                Ack(); 
                break;       
            case 'M': 
                Serial.write((byte)(1100 >> 8)); 
                Serial.write((byte)(1100 & 255)); 
                Ack(); 
                break;  
            default: 
                while(Serial.available()) Serial.read(); 
                Nack(); 
                break;
        }
    }
}

bool Fill() {
    uint8_t head[4];
    if (Serial.readBytes(head, 4) != 4) return false;
    word firstLed = (head[0] << 8) | head[1];
    word numberOfLeds = (head[2] << 8) | head[3];
    uint8_t rgb[3];
    if (Serial.readBytes(rgb, 3) != 3) return false;
    RgbColor color(rgb[0], rgb[1], rgb[2]);

    for (word i = 0; i < numberOfLeds; i++) { 
        word dofIndex = firstLed + i;
        
        word channel = dofIndex / dofBlockSize;
        word ledIndex = dofIndex % dofBlockSize;
        
        if (channel < ChannelCount && ledIndex < stripLengths[channel]) {
            strips[channel]->SetPixelColor(ledIndex, color);
        }
    }
    Ack();
    return true;
}

bool ReceiveData() {
    uint8_t head[4];
    if (Serial.readBytes(head, 4) != 4) return false; 
    
    word firstLed = (head[0] << 8) | head[1];
    word numberOfLeds = (head[2] << 8) | head[3];
    
    size_t bytesToRead = numberOfLeds * 3;
    if (bytesToRead > sizeof(rgbDataBuffer)) return false;
    if (Serial.readBytes(rgbDataBuffer, bytesToRead) != bytesToRead) return false;
    
    size_t bufIdx = 0;
    for (word i = 0; i < numberOfLeds; i++) { 
        uint8_t r = rgbDataBuffer[bufIdx++];
        uint8_t g = rgbDataBuffer[bufIdx++];
        uint8_t b = rgbDataBuffer[bufIdx++];
        
        word dofIndex = firstLed + i;
        
        word channel = dofIndex / dofBlockSize;
        word ledIndex = dofIndex % dofBlockSize;
        
        if (channel < ChannelCount && ledIndex < stripLengths[channel]) {
            strips[channel]->SetPixelColor(ledIndex, RgbColor(r, g, b));
        }
    }
    Ack();
    return true;
}