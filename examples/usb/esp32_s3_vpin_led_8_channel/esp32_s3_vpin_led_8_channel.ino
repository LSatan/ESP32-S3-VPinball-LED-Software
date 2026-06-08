#include <NeoPixelBus.h>

#define ChannelCount 8
#define FirmwareVersionMajor 1
#define FirmwareVersionMinor 3

// --- FEATURE SWITCH ---
// Set to 1 to have all LEDs light up in red, green, blue one after the other when turned on.
// Set to 0 to disable the test (cabinet remains dark at boot).
#define ENABLE_LED_TEST 1 

// --- YOUR PHYSICAL HARDWARE ---
const uint16_t stripLengths[ChannelCount] = { 144, 144, 0, 0, 0, 0, 0, 0 }; //your LED length per output
const uint8_t pins[ChannelCount] = { 4, 5, 6, 7, 8, 9, 10, 11 }; //Your output pins. Be careful if you want to change the pins, not all pins are compatible.
const uint16_t dofBlockSize = 1100; //If you send each channel individually, then the maximum number of the longest strip must be smaller. If you send everything over one channel, then the number must be the same or larger than all the LEDs in the system.
const uint16_t BufferSize = 8192;

typedef NeoPixelBus<NeoGrbFeature, NeoEsp32LcdX8Ws2812xMethod> MyPixelBus;
MyPixelBus* strips[ChannelCount];

uint8_t rgbDataBuffer[BufferSize]; 
uint16_t RecivedSize = 0; 

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
                        RecivedSize = (buf[0] << 8) | buf[1];
                        if (RecivedSize == 0) RecivedSize = dofBlockSize; 
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
        uint16_t channel = dofIndex / RecivedSize;
        uint16_t ledIndex = dofIndex % RecivedSize;
        
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
    uint16_t channel  = dofIndex / RecivedSize;
    uint16_t ledIndex = dofIndex % RecivedSize;

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