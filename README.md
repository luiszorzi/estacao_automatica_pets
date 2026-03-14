# 💻⚙️ Sistema Automático de Bebedouro e Comedouro para Pets

Projeto de uma estação autônoma de alimentação e hidratação para pets utilizando o microcontrolador ESP32. O sistema monitora continuamente o peso das tigelas usando células de carga e controla automaticamente:

* o enchimento da água
* a liberação de ração
* os horários de alimentação

Tudo ocorre de forma automática e segura, garantindo que o animal tenha sempre água e alimento disponíveis.

---

## ⚙️ Funcionamento do Sistema

### 💧 Bebedouro
* **Acionamento por Peso:** Monitora continuamente o peso da tigela através da balança.
* Ativa automaticamente a bomba de água somente se o peso lido estiver abaixo do nível mínimo estimado.
* Desliga a bomba imediatamente quando a balança acusa que o peso programado foi atingido.
* Utiliza um tempo de estabilização no código para evitar leituras falsas causadas pela oscilação da água.

### 🍖 Comedouro
* **Reposição sob Demanda:** Nos horários programados pelo relógio, o sistema lê a balança. O servo motor só é acionado se o peso atual na tigela estiver abaixo da porção estimada, completando apenas a quantidade que falta.
* Libera a ração com controle proporcional, garantindo que o limite de peso da tigela seja respeitado sem transbordar.
* Evita desperdícios e acúmulo excessivo, pois a balança "decide" se é necessário servir mais.

### 🕒 RTC
* Mantém os horários de alimentação salvos.
* Permite alimentar o pet em horários exatos e programados.
* Continua funcionando e contando o tempo mesmo se o sistema ficar sem energia temporariamente.

---

## 📷 Esquemático do Sistema

<p align="center">
  <img src="imagens/esquematico.png" width="650">
</p>

---

## 🛠️ Componentes Utilizados

| Componente | Quantidade |
| :--- | :---: |
| ESP32 | 1 |
| Módulo HX711 | 2 |
| Células de carga | 2 |
| Servo motor | 1 |
| Módulo Relé (1 canal) | 1 |
| Bomba de água DC | 1 |
| Módulo RTC DS3231 | 1 |
| Fonte 5V externa | 1 |

---

## 🔌 Ligações do Circuito

### 🍖 Comedouro (Servo + Balança da Ração)

**Servo Motor**
| Fio do Servo | Conexão |
| :--- | :--- |
| Sinal (Laranja) | GPIO 27 |
| VCC (Vermelho) | 5V |
| GND (Marrom/Preto) | GND |

**Balança da Ração (HX711)**
| Pino HX711 | ESP32 |
| :--- | :--- |
| VCC | 3V3 |
| GND | GND |
| DT | GPIO 32 |
| SCK | GPIO 33 |

**Célula de Carga da Ração**
| Fio | HX711 |
| :--- | :--- |
| Vermelho | E+ |
| Preto | E- |
| Branco | A- |
| Verde | A+ |

### 💧 Bebedouro (Bomba + Relé + Balança da Água)

**Relé (Controle)**
| Pino Relé | ESP32 |
| :--- | :--- |
| VCC | 5V |
| GND | GND |
| IN | GPIO 26 |

**Relé (Potência da Bomba)**
| Conexão | Ligação |
| :--- | :--- |
| COM | Positivo (+) da fonte 5V |
| NO | Positivo (+) da bomba |
| Negativo da bomba | GND da fonte |

**Balança da Água (HX711)**
| Pino HX711 | ESP32 |
| :--- | :--- |
| VCC | 3V3 |
| GND | GND |
| DT | GPIO 18 |
| SCK | GPIO 19 |

### 🕒 Módulo RTC DS3231

| Pino RTC | ESP32 |
| :--- | :--- |
| SDA | GPIO 21 |
| SCL | GPIO 22 |
| VCC | 3V3 |
| GND | GND |

---

## 💻 Código

O firmware foi desenvolvido em Arduino/C++ para ESP32. Principais bibliotecas utilizadas:
* `HX711`
* `ESP32Servo`
* `RTClib`
* `Wire`
