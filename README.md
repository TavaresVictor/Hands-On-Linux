# DevTITANS 05 — HandsOn Linux — Equipe 0X

Projeto de integração entre um ESP32 e um módulo USB/serial CP2102. O
firmware implementa uma SmartLamp e o driver Linux disponibiliza os comandos
por arquivos em `/sys/kernel/smartlamp`.

## Integrantes

Em ordem alfabética:

1. Ademar Castro
2. Ivan Marcos
3. João Tavares
4. Pedro Sadarc
5. Silas Filho

Contato: [ademar.castro@icomp.ufam.edu.br](mailto:ademar.castro@icomp.ufam.edu.br)

## Componentes

- `smartlamp.ino`: firmware Arduino para ESP32.
- `diagram.json`: circuito pronto para ser montado no simulador Wokwi.
- `smartlamp-kernel-module/smartlamp.c`: driver USB completo com interface
  sysfs.
- `smartlamp-kernel-module/probe.c`, `serial_write.c`, `serial_read.c` e
  `sysfs.c`: versões isoladas das etapas da atividade, selecionáveis pelo
  `Makefile`.

## Pinagem

| Função | ESP32 | Simulação |
|---|---:|---|
| LED/PWM | GPIO 2 | LED externo com resistor de 220 Ω |
| LDR/ADC | GPIO 34 | Saída `AO` do `wokwi-photoresistor-sensor` |
| Alimentação do LDR | 3V3 e GND | 3V3 e GND da placa |

O valor do LED é uma porcentagem de 0 a 100. O valor do LDR também é
normalizado para 0 a 100.

## Protocolo serial

A comunicação usa 9600 baud, 8N1, e cada mensagem termina com `\n`:

```text
SET_LED 80  -> RES SET_LED 80
GET_LED     -> RES GET_LED 80
GET_LDR     -> RES GET_LDR 45
```

O firmware também publica `RES GET_LDR <valor>` periodicamente, a cada dois
segundos. O driver ignora respostas que pertencem a outro comando.

## Teste online no Wokwi

1. Crie um projeto **ESP32 DevKit v1** em [Wokwi](https://wokwi.com/).
2. Copie o conteúdo de `smartlamp.ino` para o sketch.
3. Substitua o diagrama pelo conteúdo de `diagram.json` deste repositório.
4. Inicie a simulação e abra o monitor serial em 9600 baud.
5. Envie, por exemplo, `SET_LED 75`, `GET_LED` ou `GET_LDR`.

Essa modalidade substitui a placa física para demonstrar o firmware e o
protocolo. Um navegador não expõe um dispositivo USB CP2102 para um módulo de
kernel; portanto, a parte do driver precisa ser compilada e executada em uma
máquina Linux com um ESP32 conectado por USB, ou validada com um dispositivo
serial compatível.

## Firmware em placa física

No Arduino IDE, abra `smartlamp.ino`, selecione **ESP32 Dev Module** (ou
**Node32s**), escolha a porta serial e faça o upload. O firmware usa o mesmo
protocolo da simulação.

## Compilação do driver Linux

É necessário ter os headers do kernel em uso, GCC e Make:

```sh
cd smartlamp-kernel-module
make
```

O resultado é `smartlamp.ko`. Para compilar uma etapa isolada:

```sh
make MODULE=probe
make MODULE=serial_write
make MODULE=serial_read
make MODULE=sysfs
```

O driver está configurado para o CP2102, com `Vendor ID 0x10C4` e `Product
ID 0xEA60`. Como o Linux normalmente já possui o driver genérico `cp210x`,
ele pode estar usando o dispositivo. Nesse caso, desassocie a interface do
`cp210x` antes de carregar este módulo, conforme o identificador mostrado por
`lsusb` e em `/sys/bus/usb/devices/`.

```sh
sudo insmod smartlamp.ko
dmesg | tail -n 20
```

## Uso pelo sysfs

```sh
echo 80 | sudo tee /sys/kernel/smartlamp/led
cat /sys/kernel/smartlamp/led
cat /sys/kernel/smartlamp/ldr
```

Para remover o módulo:

```sh
sudo rmmod smartlamp
```

O arquivo `ldr` é somente leitura. Valores de LED fora do intervalo 0–100
retornam erro.

## Licença

O driver é distribuído sob GPL, conforme declarado no código-fonte.
