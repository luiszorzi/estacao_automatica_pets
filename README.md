# 💻⚙️ Sistema Inteligente de Bebedouro e Comedouro para Pets

Projeto de uma estação autônoma de alimentação e hidratação para pets utilizando o microcontrolador ESP32. O sistema monitora continuamente o peso das tigelas usando células de carga e controla automaticamente:

* o enchimento da água;
* a liberação de ração;
* os horários de alimentação;
* o registro de eventos na nuvem.

Tudo ocorre de forma automática e segura, garantindo que o animal tenha sempre água e alimento disponíveis.

---

## ⚙️ Funcionamento do Sistema

### 💧 Bebedouro

* **Acionamento por Peso:** monitora continuamente o peso da tigela através da balança.
* Ativa automaticamente a bomba de água somente se o peso lido estiver abaixo do nível mínimo configurado.
* Desliga a bomba quando o peso programado é atingido.
* Utiliza tempo de estabilização para evitar leituras falsas causadas pela movimentação da água.
* Detecta a remoção da tigela e interrompe imediatamente o enchimento.

### 🍖 Comedouro

* **Reposição sob Demanda:** nos horários programados pelo relógio RTC, o sistema verifica o peso atual da tigela.
* O servo motor é acionado apenas quando necessário, completando a quantidade de ração faltante.
* Utiliza dosagem proporcional para evitar excesso de alimento.
* Possui detecção de falha caso a balança não registre aumento de peso após várias tentativas.

### 🕒 RTC

* Mantém os horários de alimentação armazenados.
* Permite refeições em horários exatos e programados.
* Continua funcionando mesmo após quedas temporárias de energia graças à bateria do módulo.

### ☁️ Monitoramento em Nuvem

* Conecta-se à rede Wi-Fi.
* Registra automaticamente eventos no ThingSpeak.
* Armazena a quantidade de água e ração adicionada em cada operação.
* Permite acompanhamento remoto através de gráficos e histórico de consumo.

### 💾 Backup Offline

* Caso não haja conexão com a internet ou o ThingSpeak esteja indisponível, os dados não são perdidos.
* Os registros são armazenados localmente na memória interna do ESP32 utilizando LittleFS.
* Os eventos são salvos em formato CSV contendo:

  * data;
  * horário;
  * tipo do evento (ÁGUA ou RAÇÃO);
  * quantidade adicionada.

---

## 🛡️ Recursos de Segurança

* Detecção de remoção da tigela de água.
* Verificação de estabilização das leituras das balanças.
* Proteção contra transbordamento.
* Verificação da presença da tigela de ração.
* Detecção de falhas na saída de ração.
* Desligamento do bebedouro durante o processo de alimentação.

---

## 📷 Esquemático do Sistema

<p align="center">
  <img src="imagens/esquematico.png" width="650">
</p>

---

## 🛠️ Componentes Utilizados

| Componente            | Quantidade |
| :-------------------- | :--------: |
| ESP32                 |      1     |
| Módulo HX711          |      2     |
| Células de carga      |      2     |
| Servo motor           |      1     |
| Módulo Relé (1 canal) |      1     |
| Bomba de água DC      |      1     |
| Módulo RTC DS3231     |      1     |
| Fonte 5V externa      |      1     |

---

## 🔌 Ligações do Circuito

### 🍖 Comedouro (Servo + Balança da Ração)

**Servo Motor**

| Fio do Servo       | Conexão |
| :----------------- | :------ |
| Sinal (Laranja)    | GPIO 27 |
| VCC (Vermelho)     | 5V      |
| GND (Marrom/Preto) | GND     |

**Balança da Ração (HX711)**

| Pino HX711 | ESP32   |
| :--------- | :------ |
| VCC        | 3V3     |
| GND        | GND     |
| DT         | GPIO 32 |
| SCK        | GPIO 33 |

**Célula de Carga da Ração**

| Fio      | HX711 |
| :------- | :---- |
| Vermelho | E+    |
| Preto    | E-    |
| Branco   | A-    |
| Verde    | A+    |

### 💧 Bebedouro (Bomba + Relé + Balança da Água)

**Relé (Controle)**

| Pino Relé | ESP32   |
| :-------- | :------ |
| VCC       | 5V      |
| GND       | GND     |
| IN        | GPIO 26 |

**Relé (Potência da Bomba)**

| Conexão           | Ligação                  |
| :---------------- | :----------------------- |
| COM               | Positivo (+) da fonte 5V |
| NO                | Positivo (+) da bomba    |
| Negativo da bomba | GND da fonte             |

**Balança da Água (HX711)**

| Pino HX711 | ESP32   |
| :--------- | :------ |
| VCC        | 3V3     |
| GND        | GND     |
| DT         | GPIO 25 |
| SCK        | GPIO 14 |

### 🕒 Módulo RTC DS3231

| Pino RTC | ESP32   |
| :------- | :------ |
| SDA      | GPIO 21 |
| SCL      | GPIO 22 |
| VCC      | 3V3     |
| GND      | GND     |

---

## 💻 Código

O firmware foi desenvolvido em Arduino/C++ para ESP32.

### Bibliotecas Utilizadas

* HX711
* ESP32Servo
* RTClib
* Wire
* WiFi
* HTTPClient
* LittleFS

---

## 📊 Fluxo de Funcionamento

1. O ESP32 inicializa sensores, RTC, Wi-Fi e memória interna.
2. As balanças monitoram continuamente o peso das tigelas.
3. Quando necessário:

   * a bomba reabastece a água;
   * o servo libera ração.
4. O RTC controla os horários das refeições.
5. Os eventos são enviados ao ThingSpeak.
6. Caso não haja internet, os dados são armazenados localmente no LittleFS.
