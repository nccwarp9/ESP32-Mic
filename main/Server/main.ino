#include <WiFi.h>
#include <math.h>
#include <driver/adc.h>
#include <SimpleRingBuffer.h>

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

#define SERIAL_DEBUG_ON true
#define TCP_SERVER_PORT 8088
#define AUDIO_BUFFER_MAX 16000  //8192
#define SINGLE_PACKET_MIN 512
#define SINGLE_PACKET_MAX 1024
SimpleRingBuffer audio_buffer;

/*
.##.....##....###....########.
.##.....##...##.##...##.....##
.##.....##..##...##..##.....##
.##.....##.##.....##.########.
..##...##..#########.##...##..
...##.##...##.....##.##....##.
....###....##.....##.##.....##
*/
volatile int counter = 0;
unsigned int lastLog = 0;
unsigned long lastSend = millis();
unsigned int lastClientCheck = 0;

uint8_t txBuffer[SINGLE_PACKET_MAX + 1];

const char* ssid     = "cupi";
const char* password = "*";

WiFiClient audioClient;
WiFiClient checkClient;
WiFiServer audioServer(TCP_SERVER_PORT);

/*
.########.####.##.....##.########.########.
....##.....##..###...###.##.......##.....##
....##.....##..####.####.##.......##.....##
....##.....##..##.###.##.######...########.
....##.....##..##.....##.##.......##...##..
....##.....##..##.....##.##.......##....##.
....##....####.##.....##.########.##.....##
*/
hw_timer_t * timer = NULL;
volatile SemaphoreHandle_t timerSemaphore;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

void IRAM_ATTR onTimer(){
  // Increment the counter and set the time of ISR
  portENTER_CRITICAL_ISR(&timerMux);
  
  readMic(); 
  
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
    int mySampleRate = AUDIO_TIMING_VAL;
/*
.##......##.####.########.####
.##..##..##..##..##........##.
.##..##..##..##..##........##.
.##..##..##..##..######....##.
.##..##..##..##..##........##.
.##..##..##..##..##........##.
..###..###..####.##.......####
*/
    Serial.begin(115200);
    WiFi.begin(ssid, password);
    Serial.print("Connecting to ");
    Serial.println(ssid);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi connected with IP address: ");
    Serial.println(WiFi.localIP());

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
    
    /* start Server */
    audio_buffer.init(AUDIO_BUFFER_MAX);
    audioServer.begin();

    timerSemaphore = xSemaphoreCreateBinary();
    timer = timerBegin(0, 80, true);
    timerAttachInterrupt(timer, &onTimer, true);
    timerAlarmWrite(timer, mySampleRate, true);
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

    if (xSemaphoreTake(timerSemaphore, 0) == pdTRUE){
        portENTER_CRITICAL(&timerMux);
        portEXIT_CRITICAL(&timerMux);
        // Print it
        Serial.print(".");
    }

    // CHECK IF CONNECTED
    if ((now - lastClientCheck) > 100) {
        lastClientCheck = now;
        checkClient = audioServer.available();
        if (checkClient) {
            audioClient = checkClient;
        }
    }

/*
.########..########.########..##.....##..######..
.##.....##.##.......##.....##.##.....##.##....##.
.##.....##.##.......##.....##.##.....##.##.......
.##.....##.######...########..##.....##.##...####
.##.....##.##.......##.....##.##.....##.##....##.
.##.....##.##.......##.....##.##.....##.##....##.
.########..########.########...#######...######..
*/
    #if SERIAL_DEBUG_ON
    if ((now - lastLog) > 1000) {
        lastLog = now;
        Serial.println("counter was " + String(counter));
        Serial.println("audio buffer size is now " + String(audio_buffer.getSize()));
        counter = 0;
    }
    #endif

    // SEND DATA
    sendEvery(100);
}

/*
.########..########....###....########.....##.....##.####..######.
.##.....##.##.........##.##...##.....##....###...###..##..##....##
.##.....##.##........##...##..##.....##....####.####..##..##......
.########..######...##.....##.##.....##....##.###.##..##..##......
.##...##...##.......#########.##.....##....##.....##..##..##......
.##....##..##.......##.....##.##.....##....##.....##..##..##....##
.##.....##.########.##.....##.########.....##.....##.####..######.
*/
void readMic(void) {
    //read audio from ESP32 ADC 
    uint16_t value = adc1_get_voltage(ADC1_CHANNEL_0);
   // value = map(value, 0, 4095, 0, 255);
    audio_buffer.put(value);
    counter++;
}

/*
..######..########.##....##.########........###....##.....##.########..####..#######.
.##....##.##.......###...##.##.....##......##.##...##.....##.##.....##..##..##.....##
.##.......##.......####..##.##.....##.....##...##..##.....##.##.....##..##..##.....##
..######..######...##.##.##.##.....##....##.....##.##.....##.##.....##..##..##.....##
.......##.##.......##..####.##.....##....#########.##.....##.##.....##..##..##.....##
.##....##.##.......##...###.##.....##....##.....##.##.....##.##.....##..##..##.....##
..######..########.##....##.########.....##.....##..#######..########..####..#######.
*/
void sendEvery(int delay) {
    // if it's been longer than 100ms since our last broadcast, then broadcast.
    if ((millis() - lastSend) >= delay) {
        sendAudio();
        lastSend = millis();
    }
}

void sendAudio(void) {
    if (!audioClient) {
        return;
    }
    int count = 0;
    int storedSoundBytes = audio_buffer.getSize();

    // don't read out more than the max of our ring buffer
    // remember, we're also recording while we're sending
    while (count < storedSoundBytes) {
        if (audio_buffer.getSize() < SINGLE_PACKET_MIN) {
            break;
        }
        // read out max packet size at a time
        // for loop should be faster, since we can check our buffer size just once?
        int size = _min(audio_buffer.getSize(), SINGLE_PACKET_MAX);
        int c = 0;
        for(int c = 0; c < size; c++) {
            txBuffer[c] = audio_buffer.get();
        }
        count += size;
        // send it!
        audioClient.write(txBuffer, size);
    }
}
