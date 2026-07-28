/*
 * SmartLamp - firmware para ESP32
 *
 * Protocolo de linha (9600 8N1):
 *   SET_LED <0..100>  -> RES SET_LED <valor>
 *   GET_LED            -> RES GET_LED <valor>
 *   GET_LDR            -> RES GET_LDR <0..100>
 *
 * O firmware tambem publica o LDR periodicamente. Isso permite testar a
 * leitura serial mesmo antes de enviar um comando pelo driver.
 */
const uint8_t LED_PIN = 2;       // LED onboard/externo usado no simulador
const uint8_t LDR_PIN = 4;      // GPIO somente entrada e ADC no ESP32
const int LDR_MAX_READING = 4095;
const unsigned long TELEMETRY_INTERVAL_MS = 2000;

const int LED_CHANNEL = 0;       // Canal PWM (0 a 15 no ESP32)
const int LED_FREQ = 5000;       // Frequencia de 5000 Hz
const int LED_RESOLUTION = 8;    // Resolucao de 8 bits (0 a 255)

int ledValue = 10;               // intensidade em porcentagem
int ldrMax = LDR_MAX_READING;
unsigned long lastTelemetry = 0;
String serialLine;

void processCommand(String command);
void ledUpdate();
int ldrGetValue();

static void sendResponse(const char *command, int value)
{
    Serial.printf("RES %s %d\n", command, value);
}

static void sendError(const char *reason)
{
    Serial.printf("ERR %s\n", reason);
}

void setup()
{
    Serial.begin(9600);
    pinMode(LED_PIN, OUTPUT);
    pinMode(LDR_PIN, INPUT);
    analogReadResolution(12);

    ledcSetup(LED_CHANNEL, LED_FREQ, LED_RESOLUTION);
    ledcAttachPin(LED_PIN, LED_CHANNEL);

    ledUpdate();
    serialLine.reserve(64);
    Serial.println("RES READY 1");
}

void loop()
{
    while (Serial.available() > 0) {
        const char character = static_cast<char>(Serial.read());
        if (character == '\n') {
            processCommand(serialLine);
            serialLine = "";
        } else if (character != '\r' && serialLine.length() < 63) {
            serialLine += character;
        } else if (serialLine.length() >= 63) {
            serialLine = "";
            sendError("COMMAND_TOO_LONG");
        }
    }
    if (millis() - lastTelemetry >= TELEMETRY_INTERVAL_MS) {
        lastTelemetry = millis();
        sendResponse("GET_LDR", ldrGetValue());
    }
}

void processCommand(String command)
{
    command.trim();
    if (command.length() == 0) {
        return;
    }
    char commandName[20] = {0};
    long parameter = 0;
    const int fields = sscanf(command.c_str(), "%19s %ld", commandName, &parameter);
    if (strcmp(commandName, "SET_LED") == 0 && fields == 2) {
        if (parameter < 0 || parameter > 100) {
            sendError("LED_RANGE");
            return;
        }
        ledValue = static_cast<int>(parameter);
        ledUpdate();
        sendResponse("SET_LED", ledValue);
    } else if (strcmp(commandName, "GET_LED") == 0 && fields == 1) {
        sendResponse("GET_LED", ledValue);
    } else if (strcmp(commandName, "GET_LDR") == 0 && fields == 1) {
        sendResponse("GET_LDR", ldrGetValue());
    } else {
        sendError("INVALID_COMMAND");
    }
}

void ledUpdate()
{
    const int pwmValue = map(ledValue, 0, 100, 0, 255);
    ledcWrite(LED_CHANNEL, constrain(pwmValue, 0, 255));
}

int ldrGetValue()
{
    const int rawValue = analogRead(LDR_PIN);
    const int maximum = (ldrMax > 0) ? ldrMax : LDR_MAX_READING;
    return constrain(map(rawValue, 0, maximum, 0, 100), 0, 100);
}
