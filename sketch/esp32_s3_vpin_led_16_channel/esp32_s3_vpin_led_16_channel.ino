#include <NeoPixelBus.h>

#define ChannelCount 16
#define FirmwareVersionMajor 1
#define FirmwareVersionMinor 3

// --- FEATURE SWITCH ---
#define ENABLE_LED_TEST 1 

// --- YOUR PHYSICAL HARDWARE ---
// 16 channels: Enter here exactly the physical lengths of your 16 strips.
const uint16_t stripLengths[ChannelCount] = { 
    144, 144, 144, 144, 144, 144, 144, 144, // Channels 0 to 7
    144, 144, 144, 144, 144, 144, 144, 144  // Channels 8 to 15
};

/// 16 channels: Enter the 16 pins here! (Check if your board has these 16 pins available as outputs)
const uint8_t pins[ChannelCount] = { 
    4, 5, 6, 7, 15, 16, 17, 18,     // Channels 0 to 7
    8, 9, 10, 11, 12, 13, 14, 48 // Channels 8 to 15
};

// Dynamic block size (Will be automatically overwritten by the 'L' command from DOF)
uint16_t dofBlockSize = 1100; 

// --- 16-CHANNEL ENGINE ---
// We are now using the X16 method of the LCD hardware engine of the ESP32-S3
typedef NeoPixelBus<NeoGrbFeature, NeoEsp32LcdX16Ws2812xMethod> MyPixelBus;
MyPixelBus* strips[ChannelCount];

// --- DOUBLE BUFFER FOR 16 CHANNELS ---
// Expanded to 16 KB so that DOF can send us data without any restrictions
uint8_t rgbDataBuffer[16384]; 

void Ack()  { Serial.write('A'); Serial.flush(); }
void Nack() { Serial.write('N'); Serial.flush(); }

void ShowAll() {
    for(uint8_t i = 0; i < ChannelCount; i++) {
        while(!strips[i]->CanShow()) { yield(); }
        strips[i]->Show();
    }
}

void setup() {
    // Double USB hardware buffer for the 16 channels
    Serial.setRxBufferSize(16384); 
    Serial.begin(115200);
    Serial.setTimeout(100); 
    
    // 1. Initialize hardware
    for(uint8_t i = 0; i < ChannelCount; i++) {
        strips[i] = new MyPixelBus(stripLengths[i], pins[i]);
        strips[i]->Begin();
        strips[i]->ClearTo(RgbColor(0));
    }
    ShowAll();
    
    // 2. LED boat test (Now works over all 16 channels!)
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
    // Sicherheitsprüfung auf unseren neuen, größeren Puffer
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