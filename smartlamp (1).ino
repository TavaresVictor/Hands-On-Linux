// Defina os pinos de LED e LDR
// Defina uma variável com valor máximo do LDR (4000)
// Defina uma variável para guardar o valor atual do LED (10)

//sudo modprobe cp210x se nao aparecer o port

int ledPin = 23;
int ledValue = 10;

int ldrPin = 4;
// Faça testes no sensor LDR para encontrar o valor máximo
// e atribua à variável ldrMax.
int ldrMax = 4000;

// Canal usado pelo PWM do LED.
int ledChannel = 0;

// Guarda os caracteres recebidos pelo Monitor Serial.
String serialCommand = "";

void processCommand(String command);
void ledUpdate();
int ldrGetValue();
bool isInteger(String value);

void setup() {
    Serial.begin(9600);

    pinMode(ledPin, OUTPUT);
    pinMode(ldrPin, INPUT);

    // O ESP32 realiza leitura analógica de 0 até 4095.
    analogReadResolution(12);

    // Configuração do PWM:
    // canal 0, frequência de 5000 Hz e resolução de 8 bits.
    ledcSetup(ledChannel, 5000, 8);
    ledcAttachPin(ledPin, ledChannel);

    // Faz o LED iniciar com intensidade 10.
    ledUpdate();

    Serial.printf("SmartLamp Initialized.\n");

    // Envia automaticamente a leitura do LDR ao conectar.
    processCommand("GET_LDR");
}


// Função loop será executada infinitamente pelo ESP32.
void loop() {
    // Obtenha os comandos enviados pela serial
    // e processe-os com a função processCommand.

    while (Serial.available() > 0) {
        char serialChar = Serial.read();

        // O comando é processado quando o usuário envia
        // uma quebra de linha pelo Monitor Serial.
        if (serialChar == '\n') {
            processCommand(serialCommand);
            serialCommand = "";
        }
        else if (serialChar != '\r') {
            serialCommand += serialChar;
        }
    }
}


void processCommand(String command) {
    // Remove espaços e quebras de linha.
    command.trim();

    // Permite que os comandos sejam enviados
    // em letras maiúsculas ou minúsculas.
    command.toUpperCase();

    if (command == "GET_LED") {
        Serial.printf("RES GET_LED %d\n", ledValue);
    }

    else if (command == "GET_LDR") {
        Serial.printf("RES GET_LDR %d\n", ldrGetValue());
    }

    else if (command.startsWith("SET_LED")) {

        // O comando precisa ter um espaço após SET_LED.
        if (!command.startsWith("SET_LED ")) {
            Serial.printf("RES SET_LED -1\n");
            return;
        }

        // Obtém o texto após "SET_LED ".
        String valueText = command.substring(8);
        valueText.trim();

        // Impede que textos como SET_LED ABC
        // sejam interpretados como zero.
        if (!isInteger(valueText)) {
            Serial.printf("RES SET_LED -1\n");
            return;
        }

        int newLedValue = valueText.toInt();

        // Somente valores entre 0 e 100 são permitidos.
        if (newLedValue >= 0 && newLedValue <= 100) {
            ledValue = newLedValue;

            ledUpdate();

            Serial.printf("RES SET_LED 1\n");
        }
        else {
            Serial.printf("RES SET_LED -1\n");
        }
    }

    else {
        Serial.printf("ERR Unknown command.\n");
    }
}


// Função para atualizar o valor do LED.
void ledUpdate() {
    // Converte o valor recebido pelo comando SET_LED
    // do intervalo 0–100 para o intervalo 0–255.

    int normalizedLedValue = map(
        ledValue,
        0,
        100,
        0,
        255
    );

    // Envia o valor normalizado para o LED.
    ledcWrite(ledChannel, normalizedLedValue);
}


// Função para ler o valor do LDR.
int ldrGetValue() {
    // Leia o sensor LDR e retorne o valor
    // normalizado entre 0 e 100.

    int ldrAnalogValue = analogRead(ldrPin);

    int normalizedLdrValue = map(
        ldrAnalogValue,
        0,
        ldrMax,
        0,
        100
    );

    // Garante que o resultado fique entre 0 e 100,
    // mesmo que a leitura ultrapasse o ldrMax.
    normalizedLdrValue = constrain(
        normalizedLdrValue,
        0,
        100
    );

    return normalizedLdrValue;
}


// Verifica se o parâmetro recebido é realmente um número inteiro.
bool isInteger(String value) {
    value.trim();

    if (value.length() == 0) {
        return false;
    }

    int firstPosition = 0;

    // Aceita o sinal para que valores negativos sejam
    // reconhecidos e posteriormente rejeitados pelo intervalo.
    if (value.charAt(0) == '-' || value.charAt(0) == '+') {
        firstPosition = 1;
    }

    if (firstPosition == value.length()) {
        return false;
    }

    for (int i = firstPosition; i < value.length(); i++) {
        if (!isDigit(value.charAt(i))) {
            return false;
        }
    }

    return true;
}
