// ===== Microrrede solar híbrida (simulação inspirada em inversor híbrido Goodwe) =====
// Pot 1 = tensão da rede (pino 35) | Pot 2 = geração FV (pino 34) | Pot 3 = consumo (pino 32)
// Relé (pino 26) = proteção CA do inversor (sobretensão / subtensão)
#include <WiFi.h>
#include <PubSubClient.h>

#define PIN_RELE   26
#define PIN_REDE   35
#define PIN_FV     34
#define PIN_CARGA  32

// ---- Wi-Fi / MQTT (rede simulada do Wokwi) ----
const char* WIFI_SSID = "Wokwi-GUEST";
const char* MQTT_HOST = "broker.hivemq.com";
const char* TOPICO    = "fiap/sprint4/goodwe-sim/telemetria"; // TROQUEM por algo único do grupo!

// ---- Parâmetros do sistema (CONFIRMAR nos datasheets Goodwe) ----
const float V_NOMINAL     = 220.0f;
const float V_TRIP_ALTO   = V_NOMINAL * 1.10f;  // desliga acima disso
const float V_VOLTA       = V_NOMINAL * 1.06f;  // histerese: religa abaixo disso
const float V_TRIP_BAIXO  = V_NOMINAL * 0.80f;  // desliga abaixo disso
const float V_VOLTA_BAIXO = V_NOMINAL * 0.85f;  // histerese: religa acima disso
const float FV_MAX_KW     = 5.0f;   // potência FV máxima
const float CARGA_MAX_KW  = 4.0f;   // consumo máximo
const float BAT_CAP_KWH   = 5.4f;   // capacidade da bateria (ex.: Lynx Home)
const float BAT_MAX_KW    = 2.5f;   // limite de carga/descarga
const float SOC_MIN       = 10.0f;  // % mínimo da bateria
const float FATOR_CO2     = 0.04f;  // kgCO2/kWh (CONFIRMAR: fator médio MCTI)
const float TARIFA        = 0.95f;  // R$/kWh (ajustar)
const float DT_H          = 0.25f;  // cada ciclo de 1 s real = 15 min simulados

// ---- Estado ----
float soc = 50.0f;               // % da bateria
bool  desarmado = false;         // true = relé aberto (proteção atuou)
float eSolarUsada = 0, eExport = 0, eImport = 0;  // kWh acumulados
unsigned long tAnterior = 0;
float horaSim = 6.0f;            // relógio simulado

WiFiClient wifi;
PubSubClient mqtt(wifi);

void conectar() {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(WIFI_SSID, "", 6);
    unsigned long t = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t < 5000) delay(100);
  }
  if (WiFi.status() == WL_CONNECTED && !mqtt.connected()) {
    mqtt.connect("esp32-goodwe-sim-grupo");
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_RELE, OUTPUT);
  digitalWrite(PIN_RELE, HIGH);  // HIGH = conectado à rede
  mqtt.setBufferSize(512);
  mqtt.setServer(MQTT_HOST, 1883);
  Serial.println("================================================================");
  Serial.println("  MICRORREDE SOLAR HIBRIDA - SIMULACAO (ref. Goodwe)");
  Serial.println("  Rele fechado: sistema conectado a rede.");
  Serial.println("================================================================");
}

void loop() {
  if (millis() - tAnterior < 1000) return;
  tAnterior = millis();
  conectar();
  mqtt.loop();

  // 1) Leituras
  float vRede  = analogRead(PIN_REDE)  / 4095.0f * 300.0f;
  float pFV    = analogRead(PIN_FV)    / 4095.0f * FV_MAX_KW;
  float pCarga = analogRead(PIN_CARGA) / 4095.0f * CARGA_MAX_KW;

  // 2) Proteção com histerese
  if (!desarmado && (vRede > V_TRIP_ALTO || vRede < V_TRIP_BAIXO)) desarmado = true;
  else if (desarmado && vRede < V_VOLTA && vRede > V_VOLTA_BAIXO) desarmado = false;
  digitalWrite(PIN_RELE, desarmado ? LOW : HIGH);

  // 3) Despacho de energia
  float pBat = 0, pRede = 0;   // pBat >0 carrega, <0 descarrega | pRede >0 exporta, <0 importa
  const char* modo;
  float energiaBat = BAT_CAP_KWH * soc / 100.0f;
  float pFVutil = desarmado ? 0.0f : pFV;   // anti-ilhamento: FV desliga junto com a rede

  float saldo = pFVutil - pCarga;
  if (saldo >= 0) {
    float espaco = BAT_CAP_KWH * (100.0f - soc) / 100.0f / DT_H;
    pBat  = fminf(saldo, fminf(BAT_MAX_KW, espaco));
    pRede = saldo - pBat;
    modo  = (pRede > 0.01f) ? "EXPORTA_REDE" : "CARREGA_BATERIA";
    if (desarmado) modo = "PROTECAO_ATIVA";
  } else {
    float deficit = -saldo;
    float disp = fmaxf(0.0f, (energiaBat - BAT_CAP_KWH * SOC_MIN / 100.0f) / DT_H);
    float desc = fminf(deficit, fminf(BAT_MAX_KW, disp));
    float faltou = deficit - desc;   // carga não atendida pela bateria
    pBat = -desc;
    if (desarmado) {
      pRede = 0;                     // rede desconectada
      modo = (faltou > 0.01f) ? "SEM_ENERGIA(PROTECAO)" : "USA_BATERIA(PROTECAO)";
    } else {
      pRede = -faltou;               // importa da rede o que faltou
      modo = (faltou > 0.01f) ? "IMPORTA_REDE" : "USA_BATERIA";
    }
  }

  soc += (pBat * DT_H) / BAT_CAP_KWH * 100.0f;
  soc = constrain(soc, 0.0f, 100.0f);

  // 4) Acumuladores e impacto
  float solarConsumida = fminf(pFVutil, pCarga) + fmaxf(0.0f, pBat);  // FV direto + FV p/ bateria
  eSolarUsada += solarConsumida * DT_H;
  if (pRede > 0) eExport += pRede * DT_H; else eImport += -pRede * DT_H;
  float co2 = eSolarUsada * FATOR_CO2;
  float economia = eSolarUsada * TARIFA;

  horaSim += DT_H; if (horaSim >= 24) horaSim -= 24;

  // 5) MQTT (JSON para o dashboard)
  char json[512];
  snprintf(json, sizeof(json),
    "{\"hora\":%.2f,\"v_rede\":%.1f,\"pv_kw\":%.2f,\"carga_kw\":%.2f,\"soc\":%.1f,"
    "\"bat_kw\":%.2f,\"rede_kw\":%.2f,\"rele\":\"%s\",\"modo\":\"%s\","
    "\"kwh_solar\":%.2f,\"kwh_exp\":%.2f,\"kwh_imp\":%.2f,\"co2_kg\":%.3f,\"economia_rs\":%.2f}",
    horaSim, vRede, pFVutil, pCarga, soc, pBat, pRede,
    desarmado ? "ABERTO" : "FECHADO", modo,
    eSolarUsada, eExport, eImport, co2, economia);
  if (mqtt.connected()) mqtt.publish(TOPICO, json);

  // 6) Painel legível no terminal
  int h = (int)horaSim;
  int m = (int)((horaSim - h) * 60);
  Serial.printf("[%02d:%02d] REDE %.0f V | FV %.1f kW | CARGA %.1f kW | BATERIA %.0f%%\n",
                h, m, vRede, pFVutil, pCarga, soc);
  Serial.printf("        Modo: %s | Rele: %s | MQTT: %s\n",
                modo, desarmado ? "ABERTO" : "FECHADO", mqtt.connected() ? "OK" : "offline");
  Serial.printf("        Solar usado: %.2f kWh | CO2 evitado: %.3f kg | Economia: R$ %.2f\n",
                eSolarUsada, co2, economia);
  if (desarmado) Serial.println("        *** PROTECAO ATIVA: tensao fora da faixa ***");
  Serial.println("----------------------------------------------------------------");
}
