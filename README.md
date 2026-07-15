# AntiMatter Satellite - Smart Automation Firmware

Tu microcontrolador (ESP8266 o ESP32) se convierte en un nodo IoT completo: lee sensores, controla actuadores y ejecuta reglas de automatización.

**Estado:** Funcional en ESP8266 & ESP32 | Interfaz web integrada | EEPROM para persistencia sin internet

---

## Iniciar Rápido (5 minutos)

### 1. Hardware necesario

| Componente | Necesario? |
|-----------|-----------|
| Placa ESP8266 (NodeMCU, D1 Mini) o ESP32 | Sí |
| Cable USB para primer upload | Sí |
| Sensor(es) que quieras usar (DHT22, BMP280, etc.) | Opcional |

### 2. Conectar y configurar por primera vez

**a) Instalar PlatformIO + abrir el proyecto**

[Instalar extension en VS Code](https://platformio.org/install/ide?install=vscode)

Luego abrir el directorio del proyecto con la extensión.

**b) Subir firmware al dispositivo**

```bash
pio upload -e <tu_plataforma>
# Ej: pio upload -e esp8266_d1_mini     # NodeMCU/D1 Mini
#     pio upload -e esp32_devkit        # ESP32 DevKit
```

**c) Conectar al WiFi de emergencia del dispositivo**

Busca la red `PeriferalSetup` en tus dispositivos. Ábrela — no tiene contraseña.

Luego entra a `http://192.168.4.1`, ve a **NETWORK Tab**, introduce tu SSID y contraseña. El dispositivo se reinicia y se conecta a internet.

**d) Configurar sensores y reglas**

Ahora acceso vía web: abre tu navegador, busca la IP de tu red (`http://<IP_del_dispositivo>`).

- Ve a **SETTINGS** para calibrar los sensores
- Ve a **AUTOMATIONS** para crear las primeras reglas

---

## Sensores soportados (9 tipos)

| Sensor | Rango | Unidad | Ejemplo de uso |
|--------|-------|--------|----------------|
| Temperatura | -50 a +150 | °C | DHT22, DS18B20 |
| Humedad | 0-100 | % | DHT22, humedad del suelo |
| Luz | 0-65535 | lux | Fotoresistencia, sensores de luz |
| Presión | 300-1100 | hPa | BMP280 |
| Nivel | 0-100 | % | Tanques, piscinas |
| Calidad del aire | GOOD / WARN / BAD | enum | MQ135, SDS011 |
| Lluvia | ON / OFF | bool | Detector de lluvia |
| Relay | ON / OFF | bool | Relés de control |
| Dimmer (gradiente) | 0-100 | % | PWM, luz LED, ventilador |

Cada sensor tiene calibración individual para mayor precisión.

---

## Automatización: hasta 20 reglas

Crea reglas con hasta **5 sensores** y **5 actuadores** cada una.

### Tipos de regla

- **Edge (cambio de estado):** Cuando un dispositivo pasa a ON/OFF → activar alarma
- **Threshold (umbral):** Cuando temperatura supera X° → encender ventilador. Soporta múltiples condiciones combinadas
- **Scheduled (hora fija):** Activarse a una hora específica cada día. Funciona con NTP o reloj local
- **Periodic (intervalo):** Cada N segundos — ideal para enviar datos al servidor

Opciones avanzadas en todas las reglas: delay, cooldown y tiempo mínimo ON/OFF.

---

## Control de actuadores

**Relyes:** ENCENDER / APAGAR / TOGGLE (invertir estado) / PULSE (activar por X ms y soltar). Soporta persistencia entre reinicios para recordar el último estado.

**Dimers:** Controlar luz LED, ventilador o motores con desvanecimiento suave (fade) entre valores de 0 a 100%.

---

## Comunicación: Web + Red local

### Sin cable USB — toda la configuración es web

Solo necesitas un navegador para configurar desde cero y luego el dispositivo funciona solo. El servidor HTTP corre sin internet, leyendo de EEPROM.

**Tabs principales:**
- **DEVICES** → Control real-time de actuadores
- **AUTOMATIONS** → Asistente visual para crear reglas
- **SETTINGS** → Calibrar sensores y revisar configuraciones
- **NETWORK** → Cambiar WiFi o hacer reset de fábrica

### Comandos HTTP (curl)

```bash
# Borrar WiFi guardada → vuelve a AP modo
# (no recomendado en producción sin autenticación previa)
POST /factory
```

Para más pormenores del API, consulta la sección técnica debajo.

---

## Plataformas compatibles

| Placa | Estado |
|-------|--------|
| ESP8266 (NodeMCU, D1 Mini, etc.) | ✅ Confirmado |
| ESP32 (DevKit, Generic) | ✅ Confirmado |
| ESP32-S2, ESP32-S3, ESP32-C3 | 🟡 Probable |

El código detecta automáticamente tu placa. Para agregar una nueva solo añade una sección `#define` en el código.

---

## Persistencia de datos (EEProm 4 KB)

Todo lo que configuras via web se guarda y sobrevive al reinicio, menos los últimos valores leídos de sensor (se pierden si borras EEPROM):

| ¿Qué persiste? | Tamaño |
|---------------|--------|
| Credenciales WiFi + reglas calibradas + reglas de automatización | ~2.2 KB total |
| Estado de relés (si se activa la opción persistence) | 10 B |

Reset de fábrica: pestaña **SETTINGS** → botón "Factory Reset" o HTTP POST al endpoint correspondiente. Elimina todas las configuraciones y reinicia en modo AP.

---

## Uso común

### Invernadero inteligente
```
→ Si temp > 30°C Y humedad > 80%   → encender ventilador
→ Si suelo seco (<30%)              → activar bomba de agua 5 min
→ Cada día a las 6:00               → luces encendidas
→ Cada día a las 18:00              → luces apagadas
```

### Casa inteligente
```
→ Moción detectada entre 18-22      → luces al 70% con fade de 1s
→ Sin moción después de 23:00       → luces se apagan a los 3 segundos
→ Temp < 18°C                       → calentar
```

### Riego inteligente
```
→ Zona 1 seca y no llovió          → abrir válvula + bomba
→ Lloviendo                         → todo cerrado (ahorro)
→ Temp < 5°C                        → apagar bomba (anti-congelación)
```

---

## Seguridad básica

Actualmente funciona sin autenticación. Recomendado para redes locales en casa, pero si lo expones a internet considera:

1. Usar solo en WiFi de red local confiable
2. Cambiar el SSID del AP de emergencia (`PeriferalSetup`) a algo aleatorio
3. Bloquear los puertos del router hacia afuera
4. Si necesitas acceso remoto, usa VPN o autenticación HTTP personalizada

---

## Solución rápida a problemas comunes

**El dispositivo aparece pero no se une al WiFi?**
- Revisa que el cable USB tiene datos (no solo energía) en la primera carga
- Espera 10 segundos después de encenderlo antes de buscarlo
- Verifica que tu router acepte MACs desconocidos y usa banda 2.4 GHz

**Los sensores no reportan valores?**
- Configura el sensor en el TAB **SETTINGS**, introduce un offset/calibración y guarda

---

## Configuración avanzada por código

Para ajustar límites o puertos UDP, edita los parámetros de compilación según necesites. Los valores por defecto están bien para la mayoría de casos:

```cpp
#define MAX_SENSORS          64        // máx sensores en memoria
#define MAX_RULES             20       // máx reglas guardadas en EEPROM
#define PULSE_DURATION_MS     10       // duración mínima default del pulso (relé)
```

---

## Código para agregar tus propios sensores

Si necesitas leer algo fuera de los sensores estándar, puedes registrar un nuevo sensor manualmente después de calibrarlo via web:

```cpp
float raw = analogRead(A5);           // leer tu GPIO
sensors::temperature("MiSensor", raw/4.0f);  // reportar al sistema
```

El valor aparece en el Web UI y puede usarse en reglas de automatización sin tocar más código.

---

## Contribuir

Cualquier problema, idea o mejora es bienvenido. Abre un issue o submita un pull request. Si deseas probar con la terminal del monitor:

```bash
# Instalar CLI de PlatformIO (requiere Python)
pip install platformio

# Construir + monitorizar
pio run -t upload --monitor -e <tu_plataforma>
```

---

## Roadmap

Siguientes mejoras planeadas: MQTT, Zigbee/Z-Wave, Matter, dashboard con gráficos, notificaciones por email/SMS y app móvil. Todo depende de la comunidad.

---

License: MIT — ver archivo `LICENSE`.
