const int ledPin = 23;
const int ldrPin = 4;

int valorLED = 0;
const int ledChannel = 0;   // Canal PWM (0 a 15 no ESP32)
const int freq = 5000;      // Frequência de 5000 Hz
const int resolution = 8;   // Resolução de 8 bits (0 a 255)

// Variável de calibração para o LDR (evita falhas se não atingir 4095 exatos)
int ldrMax = 4000;
void processCommand(String command);


void setup() {
    Serial.begin(9600);
    analogReadResolution(12);
    ledcSetup(ledChannel, freq, resolution);
    ledcAttachPin(ledPin, ledChannel);

    // Essencial para inicializar corretamente o pino analógico no ESP32
    pinMode(ldrPin, INPUT);
    pinMode(ledPin, OUTPUT);

    Serial.println("SmartLamp Initialized.");
}

void loop() {
    if(Serial.available() > 0){
        String comando = Serial.readStringUntil('\n');
        comando.trim();
        processCommand(comando);
    }
    delay(100); // Intervalo para estabilizar a leitura e não sobrecarregar a serial
}

void processCommand(String command) {
    if(command.startsWith("SET_LED ")){
      ledUpdate(command);
    }
    else if(command == "GET_LDR"){
        Serial.print("A luminosidade local está : ");
        Serial.print(ldrGetValue());
        Serial.println("%");
    }
    else if(command == "GET_LED"){
        Serial.print("A luminosidade LED está : ");
        Serial.print(valorLED);
        Serial.println("%");
    } else {
        Serial.println("Comando desconhecido: " + command);
    }
}

void ledUpdate(String comando) {
  Serial.println(comando);
  int posicaoEspaco = comando.indexOf(' ');
  String valorTexto = comando.substring(posicaoEspaco + 1);
  valorLED = valorTexto.toInt();

  if(valorLED < 0 || valorLED > 100){
    int brilhoPWM = map(valorLED, 0, 100, 0, 255);
    Serial.println(brilhoPWM);
    Serial.println("comando invalido - LED apenas de 0 a 100");
  }
  else{
      int brilhoPWM = map(valorLED, 0, 100, 0, 255);
      Serial.println(brilhoPWM);
      ledcWrite(ledChannel, brilhoPWM);
  }

}

int ldrGetValue() {
    int ldrValue = analogRead(ldrPin);
    int ldf_value_normalized = map(ldrValue, 0, ldrMax, 0, 100);
    // Garante que o valor fique estritamente entre 0 e 100%
    ldf_value_normalized = constrain(ldf_value_normalized, 0, 100);
    return ldf_value_normalized;
}
