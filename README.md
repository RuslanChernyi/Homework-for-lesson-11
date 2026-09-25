** Архітектура проекту:
Система побудована за безсерверною (Serverless) IoT-архітектурою із забезпеченням сквозного шифрування даних:

1. **Збір даних (Hardware Layer):**
   * Мікроконтролер **ESP32** зчитує показники температури та вологості з цифрового датчика **DHT22**.
   * Дані формуються у форматі JSON (Payload) з міткою часу.

2. **Передача даних (Transport & Security Layer):**
   * ESP32 підключається до **AWS IoT Core** через протокол **MQTT** поверх **TLS 1.2** (використовується взаємна автентифікація за допомогою унікальних X.509 клієнтських сертифікатів).
   * Дані публікуються у визначений топік: `iot-course/<name>/sensors/data`.

3. **Маршрутизація та обробка (AWS IoT Rules Engine):**
   * **Збереження телеметрії:** IoT Rule перехоплює всі вхідні повідомлення з топіку та через екшн автоматично заносує їх до безсерверної бази даних **AWS DynamoDB**.
   * **Алертинг та моніторинг:** Окреме IoT Rule з SQL-фільтром (`WHERE temperature > 28`) відстежує критичні значення. У разі перевищення порогу $28^\circ\text{C}$ подія фіксується в лог-стрімі **AWS CloudWatch Logs**.

```mermaid
graph LR
    SubGraph2[ESP32 + DHT22] -- "MQTT over TLS (Port 8883)" --> IoTCore[AWS IoT Core]
    
    subgraph AWS Cloud
        IoTCore -->|IoT Rule: All Messages| DynamoDB[(AWS DynamoDB)]
        IoTCore -->|IoT Rule: Temp > 28°C| CloudWatch[AWS CloudWatch Logs]
    end
```

Фото елементів з таблиці DynamoDB:
<img width="1913" height="904" alt="image" src="https://github.com/user-attachments/assets/403fa221-03e7-4cfd-8728-9661286c1186" />
** Фото логів з таблиці CloudWatch:
<img width="1913" height="904" alt="Screenshot From 2026-09-25 19-54-39" src="https://github.com/user-attachments/assets/5daf3ae9-44d6-42e3-9044-98b80a4db9ee" />
** Фото вмісту одного з логів:
<img width="1913" height="904" alt="image" src="https://github.com/user-attachments/assets/5117732c-8d5d-4e28-94ec-195297ab3076" />
