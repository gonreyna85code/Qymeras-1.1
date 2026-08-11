# AntiMatter Satellite — Guía de referencia

## Stack del proyecto
- **MCU:** ESP8266 / ESP32 — Arduino framework, PlatformIO
- **Persistencia:** EEPROM 4 KB (credenciales WiFi + reglas de automatización + calibración)
- **Red:** Servidor HTTP web embebido (sin internet necesario tras setup inicial); UDP broadcast + command relay
- **Sensores:** 9 tipos (temp, humedad, luz, presión, nivel, aire, lluvia, relay, dimmer) — cada uno con calibración individual
- **Automatización:** hasta 20 reglas de tipo Edge / Threshold / Scheduled / Periodic, hasta 5 sensores y 5 actuadores por regla

## Estructura de archivos (entrada al código fuente)
| Archivo | Propósito |
|---|---|
| `core.cpp` | Inicialización del MCU, conectividad WiFi/UDP, reporting básico |
| `web.cpp` | Servidor HTTP: root + handlers para WiFi, calibration, rules, actuadores, factory reset |
| `html.h` | Const C strings embebidas (CSS, HTML tabs, JS) que consume `web.cpp` |
| `html.cpp` | Implementación de funciones JS que el HTML llama (reloj, reglas) |
| `sensors.cpp` | Lectura y calibración de sensores, actuadores (relay/dimmer), pulsos, fades |
| `mesh.cpp` | Gestión mesh P2P + relay de mensajes entre dispositivos |
| `automations.cpp` | Lógica de evaluación y persistencia de reglas |

## Convenciones de compilación
```bash
pio upload -e esp8266_d1_mini     # NodeMCU/D1 Mini
pio upload -e esp32_devkit       # ESP32 DevKit
pio run -t upload --monitor -e <plataforma>  # build + serial monitor
```

### Macros importantes usadas en todo el proyecto
```cpp
#define MAX_SENSORS          64
#define MAX_RULES             20
#define BROADCAST_PORT         // puertos por defecto para UDP
#define COMMAND_PORT
#define EEPROM_SIZE            // 4 KB
```

## Convenciones de código aplicarse a todo refactoring
- **Variables y funciones con docstring:** helpers, `static`, funciones de utilidad
- **Blok comments:** código organizado en secciones lógicas separadas por comentarios
- **No eliminar variables existentes** — solo refactorizar para claridad; no borrar lógica
- **Push intermedio prohibido** — todo refactor va a un commit final para evaluar el impacto completo

## API web (endpoints HTTP)
| Método | Ruta | Acción |
|---|---|---|
| GET / | Root — envía HTML completo embebido |
| POST /save | Calienta credenciales WiFi guardadas → 303 redirect + reboot |
| GET/POST /calib | Lectura de calibración en JSON |
| POST /calib/set | Set: TIME / ref / min / max / fad / pulse / persist / avail / res / timezone |
| POST /genset/save | Guarda set de broadcast/command/interval |
| GET /rules | Lista reglas JSON (filtera sensor_count/actuator_count == 0) |
| POST /rules/set | Crea/regla, valida sensores/actuadores, tipo, comparador/threshold |
| POST /rules/delete | Borra regla por id (validar bounds antes borrar) |
| POST /factory | Reset de fábrica — borrar credenciales, relay state, config → reboot en AP |
| POST /toggle | API toggle para actuadores (id + lógica) |
| POST /dimmer | Value control dimmer (id + value) |

## Seguridad y límites conocidos
- Actualidad: sin autenticación — recomendado solo red local
- Puertos < 1024 o > 65500: resetear al default
- Interval < 5000ms o > 600000ms: resetear al default
- Calibración: valores de sensor pierden al reset EEPROM

## Notas para refactorizar web.cpp
- Empiezar con bloque `// ================= SECCIÓN =================` como en core.cpp
- Los headers #include están ya en el archivo, NO agregar nuevos sin necesidad
- Si se necesita cambiar el primer include para un header, mantener orden y compatibilidad
- El namespace `web { ... }` agrupa todo el servidor HTTP — mantenerlo
- Los handlers con docstring y comentarios lógicos por bloque
- Variables de la interfase web: NO TOCAR — solo mejorar sintaxis y legibilidad

## Roadmap futuro
MQTT · Zigbee/Z-Wave · Matter · dashboard gráfica · notificaciones email/SMS · app móvil

## Licencia MIT
