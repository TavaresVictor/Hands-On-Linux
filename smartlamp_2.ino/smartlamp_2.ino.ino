const int ledPin = 23;
const int ldrPin = 4;

const int ledChannel = 0;   // Canal PWM (0 a 15 no ESP32)
const int freq = 5000;      // Frequência de 5000 Hz
const int resolution = 8;   // Resolução de 8 bits (0 a 255)

void setup() {
    Serial.begin(9600);
    
    // Configuração antiga para ESP32 (Versão 2.x)
    ledcSetup(ledChannel, freq, resolution); // Configura o canal
    ledcAttachPin(ledPin, ledChannel);       // Vincula o pino 23 ao canal
    
    pinMode(ldrPin, INPUT);
    
    Serial.println("SmartLamp Initialized.");
}

void loop() {
    if(Serial.available() > 0){
        String comando = Serial.readStringUntil('\n');
        comando.trim(); 
        processCommand(comando);
    }
    int valorBruto = analogRead(ldrPin);
    
    Serial.print("Valor Bruto lido (0 a 4095): ");
    Serial.println(valorBruto);
}

void processCommand(String command) {
    if(command == "100"){
        ledcWrite(ledChannel, 255); // Usa o canal criado
        Serial.println("-> Comando 100 recebido! LED ligado no máximo.");
    }   
    else if(command == "0"){
        ledcWrite(ledChannel, 0);   // Usa o canal criado
        Serial.println("-> Comando 0 recebido! LED Desligado.");
    }
    else if(command == "SET_LED"){
        ledUpdate();
    }
    else if(command == "ldr"){
        Serial.print("A luminosidade local está : ");
        Serial.print(ldrGetValue());
        Serial.println("%");
    } else {
        Serial.println("Comando desconhecido: " + command);
    }
}

void ledUpdate() {
    int ldrValue = analogRead(ldrPin);
    int ldf_value_normalized = map(ldrValue, 0, 4095, 0, 255);
    ledcWrite(ledChannel, ldf_value_normalized); // Usa o canal criado
    Serial.print("-> SET_LED - Luminosidade em: ");
    Serial.print((ldf_value_normalized * 100) / 255);
    Serial.println("%");
}

int ldrGetValue() {
    int ldrValue = analogRead(ldrPin);
    int ldf_value_normalized = map(ldrValue, 0, 4095, 0, 100);
    return ldf_value_normalized;
}
