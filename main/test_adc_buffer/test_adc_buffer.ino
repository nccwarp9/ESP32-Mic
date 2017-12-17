#include <driver/adc.h>
#include <SimpleRingBuffer.h>

#define SERIAL_BAUD 115200
/*
....###....##.....##.########..####..#######.....##.....##....###....########.
...##.##...##.....##.##.....##..##..##.....##....##.....##...##.##...##.....##
..##...##..##.....##.##.....##..##..##.....##....##.....##..##...##..##.....##
.##.....##.##.....##.##.....##..##..##.....##....##.....##.##.....##.########.
.#########.##.....##.##.....##..##..##.....##.....##...##..#########.##...##..
.##.....##.##.....##.##.....##..##..##.....##......##.##...##.....##.##....##.
.##.....##..#######..########..####..#######........###....##.....##.##.....##
/*
//#define AUDIO_TIMING_VAL 125 /* 8,000 hz */
//#define AUDIO_TIMING_VAL 83 /* 12,000 hz */
#define AUDIO_TIMING_VAL 62 /* 16kHz */
//#define AUDIO_TIMING_VAL 30 /* 16,000 hz */
//#define AUDIO_TIMING_VAL 50  /* 20,000 hz */

#define AUDIO_BUFFER_MAX 16000  //8192
#define SINGLE_PACKET_MAX 1024
uint8_t txBuffer[SINGLE_PACKET_MAX + 1];
SimpleRingBuffer audio_buffer;
/*
.########.####.##.....##.########.########.
....##.....##..###...###.##.......##.....##
....##.....##..####.####.##.......##.....##
....##.....##..##.###.##.######...########.
....##.....##..##.....##.##.......##...##..
....##.....##..##.....##.##.......##....##.
....##....####.##.....##.########.##.....##
*/
unsigned int lastLog = 0;

hw_timer_t * timer = NULL;
volatile SemaphoreHandle_t timerSemaphore;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

void IRAM_ATTR onTimer(){
    // Increment the counter and set the time of ISR
    portENTER_CRITICAL_ISR(&timerMux);
  
    //read audio from ESP32 ADC 
    uint16_t value = adc1_get_voltage(ADC1_CHANNEL_0);
    // value = map(value, 0, 4095, 0, 255);
    audio_buffer.put(value);
    //Serial.println(value);

    portEXIT_CRITICAL_ISR(&timerMux);
    // Give a semaphore that we can check in the loop
    xSemaphoreGiveFromISR(timerSemaphore, NULL);
    // It is safe to use digitalRead/Write here if you want to toggle an output
}

/*
..######..########.########.##.....##.########.
.##....##.##..........##....##.....##.##.....##
.##.......##..........##....##.....##.##.....##
..######..######......##....##.....##.########.
.......##.##..........##....##.....##.##.......
.##....##.##..........##....##.....##.##.......
..######..########....##.....#######..##.......
 */
void setup() {
    Serial.begin(SERIAL_BAUD);
/* 
..######..########.########.##.....##.########........###....########...######.
.##....##.##..........##....##.....##.##.....##......##.##...##.....##.##....##
.##.......##..........##....##.....##.##.....##.....##...##..##.....##.##......
..######..######......##....##.....##.########.....##.....##.##.....##.##......
.......##.##..........##....##.....##.##...........#########.##.....##.##......
.##....##.##..........##....##.....##.##...........##.....##.##.....##.##....##
..######..########....##.....#######..##...........##.....##.########...######. 
*/
    adc1_config_width(ADC_WIDTH_12Bit);
    adc1_config_channel_atten(ADC1_CHANNEL_0,ADC_ATTEN_11db);

    audio_buffer.init(AUDIO_BUFFER_MAX);

// SETUP TIMER
    timerSemaphore = xSemaphoreCreateBinary();
    timer = timerBegin(0, 80, true);
    timerAttachInterrupt(timer, &onTimer, true);
    timerAlarmWrite(timer, AUDIO_TIMING_VAL, true);
    timerAlarmEnable(timer);
}

/*
.##........#######...#######..########.
.##.......##.....##.##.....##.##.....##
.##.......##.....##.##.....##.##.....##
.##.......##.....##.##.....##.########.
.##.......##.....##.##.....##.##.......
.##.......##.....##.##.....##.##.......
.########..#######...#######..##.......
*/
void loop() {
    unsigned int now = millis();
    int count = 0;
    
    if (xSemaphoreTake(timerSemaphore, 0) == pdTRUE){
        portENTER_CRITICAL(&timerMux);
        portEXIT_CRITICAL(&timerMux);
        // Print it
        //Serial.print(".");
    }

    if ((now - lastLog) > 1000) {
        lastLog = now;
        int storedSoundBytes = audio_buffer.getSize();
        Serial.println("audio buffer size is now " + String(storedSoundBytes));
        while (count < storedSoundBytes) {
          int size = _min(audio_buffer.getSize(), SINGLE_PACKET_MAX);
          for(int c = 0; c < size; c++) {
            txBuffer[c] = audio_buffer.get();
          }
          count += size;
        }
    }
}
