#include <NeoPixelBus.h>

#define ChannelCount 16
#define FirmwareVersionMajor 1
#define FirmwareVersionMinor 3

// --- FEATURE SWITCH ---
#define ENABLE_LED_TEST 1 

// --- YOUR PHYSICAL HARDWARE ---
// 16 channels: Enter here exactly the physical lengths of your 16 strips.
const uint16_t stripLengths[ChannelCount] = { 
    144, 144, 0, 0, 0, 0, 0, 0, // Channels 0 to 7
    0, 0, 0, 0, 0, 0, 0, 0  // Channels 8 to 15
};

/// 16 channels: Enter the 16 pins here! (Check if your board has these 16 pins available as outputs)
const uint8_t pins[ChannelCount] = { 
    4, 5, 6, 7, 15, 16, 17, 18,     // Channels 0 to 7
    8, 9, 10, 11, 12, 13, 14, 48 // Channels 8 to 15
};

 // If you send each channel individually, then the maximum number of the longest strip must be smaller. If you send everything over one channel, then the number must be the same or larger than all the LEDs in the system.
const uint16_t dofBlockSize = 1100;
const uint16_t BufferSize = 8192;

// --- 16-CHANNEL ENGINE ---
// We are now using the X16 method of the LCD hardware engine of the ESP32-S3
typedef NeoPixelBus<NeoGrbFeature, NeoEsp32LcdX16Ws2812xMethod> MyPixelBus;
MyPixelBus* strips[ChannelCount];


uint8_t rgbDataBuffer[BufferSize]; 
uint16_t ReceivedSize = 0; 

void Ack() {
    while (!Serial); 
    Serial.write('A');
}

void ShowAll() {
    for(uint8_t i = 0; i < ChannelCount; i++) {
        strips[i]->Show();
    }
}

void canShow(){
    bool isReady = false;
    while (!isReady){
        isReady = true;
        for(uint8_t i = 0; i < ChannelCount; i++) {
            if (!strips[i]->CanShow()){
                isReady = false;
                break;
            }
        }
    }

}

void setup() {
    Serial.setRxBufferSize(BufferSize);
    Serial.begin(115200);
    Serial.setTimeout(1000); 

    for(uint8_t i = 0; i < ChannelCount; i++) {
        strips[i] = new MyPixelBus(stripLengths[i], pins[i]);
        strips[i]->Begin();
        strips[i]->ClearTo(RgbColor(0));
    }
    ShowAll();
    
    #if ENABLE_LED_TEST
        uint32_t start = millis();
        while (millis() - start < 500) {
            if (Serial.available() > 0) return;
            yield();
        }
        
        for(uint8_t i=0; i<ChannelCount; i++) strips[i]->ClearTo(RgbColor(255, 0, 0));
        ShowAll(); delay(500);
        
        for(uint8_t i=0; i<ChannelCount; i++) strips[i]->ClearTo(RgbColor(0, 255, 0));
        ShowAll(); delay(500);
        
        for(uint8_t i=0; i<ChannelCount; i++) strips[i]->ClearTo(RgbColor(0, 0, 255));
        ShowAll(); delay(500);
        
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
                canShow();
                ShowAll();
                break;
            case 'C': 
                for(uint8_t i = 0; i < ChannelCount; i++) {
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
        
        if (channel < ChannelCount && ledIndex < stripLengths[channel]) {
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

        if (channel < ChannelCount && ledIndex < stripLengths[channel]) {
            strips[channel]->SetPixelColor(ledIndex, RgbColor(r, g, b));
        }
        ledIndex ++;
    }
    return true;
}