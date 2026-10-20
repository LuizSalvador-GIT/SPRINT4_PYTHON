// ===== Estação de recarga de veículos elétricos com solar + bateria (simulação) =====
// Referência de produtos: inversor híbrido e carregador veicular Goodwe (confirmar modelos/potências).
//
// Entradas no Wokwi:
//   Pot 1 (pino 35) = tensão da rede      | Pot 2 (pino 34) = nível de sol/nuvens (100% = céu limpo)
//   Chave 1 (pino 27) = veículo no carregador 1 | Chave 2 (pino 14) = veículo no carregador 2
//   Chave 3 (pino 13) = forçar horário de ponta
// Saídas: relé (26) = proteção CA | relé 2 (25) = contator dos painéis (corta sem sol)
//         LED 1 (18) e LED 2 (19) = carregador entregando energia
#include <WiFi.h>
#include <PubSubClient.h>

#define PIN_RELE   26
#define PIN_REDE   35
#define PIN_FV     34
#define PIN_RELE_FV 25
#define PIN_PONTA  13

const int PIN_SW[2]  = {27, 14};
const int PIN_LED[2] = {18, 19};

// ---- Wi-Fi / MQTT (rede simulada do Wokwi) ----
const char* WIFI_SSID = "Wokwi-GUEST";
const char* MQTT_HOST = "broker.hivemq.com";
const char* TOPICO    = "fiap/sprint4/goodwe-sim/telemetria"; // igual ao do dashboard.html

// ---- Parâmetros da estação (CONFIRMAR nos datasheets e na norma) ----
const float V_NOMINAL      = 220.0f;
const float V_TRIP_ALTO    = V_NOMINAL * 1.10f;
const float V_VOLTA        = V_NOMINAL * 1.06f;
const float V_TRIP_BAIXO   = V_NOMINAL * 0.80f;
const float V_VOLTA_BAIXO  = V_NOMINAL * 0.85f;
const float FV_MAX_KW      = 30.0f;  // usina solar da estação
const float CHG_MAX_KW     = 22.0f;  // potência de cada carregador
const float EV_CAP_KWH     = 60.0f;  // bateria do veículo
const float BAT_CAP_KWH    = 30.0f;  // bateria estacionária
const float BAT_MAX_KW     = 15.0f;  // limite de carga/descarga
const float SOC_MIN        = 10.0f;  // % mínimo da bateria estacionária
const float GRID_LIMIT_KW  = 30.0f;  // demanda máxima contratada (importação da rede)
const float FATOR_CO2      = 0.04f;  // kgCO2/kWh (CONFIRMAR: fator médio MCTI)
const float TARIFA_FP      = 0.80f;  // R$/kWh fora de ponta (ajustar)
const float TARIFA_P       = 2.00f;  // R$/kWh na ponta (ajustar)
const float KM_POR_KWH     = 6.0f;   // autonomia média do veículo (estimativa)
const float PV_CORTE_KW    = 0.5f;   // abaixo disso os painéis são desconectados
const float PV_RELIGA_KW   = 1.0f;   // acima disso os painéis são reconectados (histerese)
const float DT_H           = 0.25f;  // 1 s real = 15 min simulados
const bool  SOL_AUTOMATICO = true;   // true: a geração segue a curva do dia (zero à noite); false: o pot controla direto

// ---- Estado ----
float soc = 50.0f;                    // bateria estacionária (%)
bool  pvConectado = true;             // contator dos painéis
bool  desarmado = false;              // relé aberto
bool  plugado[2] = {false, false};
float evSoc[2]   = {0, 0};            // carga dos veículos (%)
float cKw[2]     = {0, 0};            // potência entregue a cada carregador
float eVeic = 0, eLimpa = 0, economia = 0, picoKw = 0;
int   sessoes = 0;
unsigned long tAnterior = 0;
float horaSim = 9.0f;            // começa de manhã, com o sol já subindo

WiFiClient wifi;
PubSubClient mqtt(wifi);

void conectar() {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(WIFI_SSID, "", 6);
    unsigned long t = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t < 5000) delay(100);
  }
  if (WiFi.status() == WL_CONNECTED && !mqtt.connected()) {
    mqtt.connect("esp32-estacao-ev-grupo");
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_RELE, OUTPUT);
  digitalWrite(PIN_RELE, HIGH);   // HIGH = conectado à rede
  pinMode(PIN_RELE_FV, OUTPUT);
  digitalWrite(PIN_RELE_FV, HIGH);
  for (int i = 0; i < 2; i++) { pinMode(PIN_SW[i], INPUT_PULLUP); pinMode(PIN_LED[i], OUTPUT); }
  pinMode(PIN_PONTA, INPUT_PULLUP);
  mqtt.setBufferSize(768);
  mqtt.setServer(MQTT_HOST, 1883);
  Serial.println("================================================================");
  Serial.println("  ESTACAO DE RECARGA EV: SOLAR + BATERIA (ref. Goodwe)");
  Serial.println("================================================================");
}

void loop() {
  if (millis() - tAnterior < 1000) return;
  tAnterior = millis();
  conectar();
  mqtt.loop();

  // 1) Leituras
  float vRede = analogRead(PIN_REDE) / 4095.0f * 300.0f;
  float nivelSol = analogRead(PIN_FV) / 4095.0f;              // 0 a 1 (nuvens)
  float curva = 1.0f;                                          // curva solar do dia: nasce às 6h, põe às 18h
  if (SOL_AUTOMATICO) curva = (horaSim > 6.0f && horaSim < 18.0f) ? sinf((horaSim - 6.0f) / 12.0f * 3.14159f) : 0.0f;
  float pFV   = nivelSol * curva * FV_MAX_KW;
  // Sem geração: abre o contator dos painéis (histerese para não ficar ligando e desligando)
  if (pvConectado && pFV < PV_CORTE_KW) pvConectado = false;
  else if (!pvConectado && pFV > PV_RELIGA_KW) pvConectado = true;
  bool ponta  = (digitalRead(PIN_PONTA) == LOW) || (horaSim >= 18.0f && horaSim < 21.0f);
  for (int i = 0; i < 2; i++) {
    bool p = (digitalRead(PIN_SW[i]) == LOW);        // chave ligada = veículo plugado
    if (p && !plugado[i]) { evSoc[i] = (float)random(15, 45); sessoes++; }
    plugado[i] = p;
  }

  // 2) Proteção de tensão com histerese
  if (!desarmado && (vRede > V_TRIP_ALTO || vRede < V_TRIP_BAIXO)) desarmado = true;
  else if (desarmado && vRede < V_VOLTA && vRede > V_VOLTA_BAIXO) desarmado = false;
  digitalWrite(PIN_RELE, desarmado ? LOW : HIGH);

  // 3) Quanto cada veículo pede (cai perto de 100% da bateria, como no carregamento real)
  float req[2], total = 0;
  for (int i = 0; i < 2; i++) {
    req[i] = 0;
    if (!desarmado && plugado[i] && evSoc[i] < 99.9f) {
      float r = (evSoc[i] < 80.0f) ? CHG_MAX_KW : CHG_MAX_KW * fmaxf(0.15f, (100.0f - evSoc[i]) / 20.0f);
      r = fminf(r, (100.0f - evSoc[i]) / 100.0f * EV_CAP_KWH / DT_H);
      req[i] = r; total += r;
    }
  }

  // 4) Despacho: sol primeiro; na ponta usa bateria antes da rede; limite de demanda da rede
  bool pvOn = pvConectado && !desarmado;   // painéis ligados só com sol e rede dentro da faixa
  digitalWrite(PIN_RELE_FV, pvOn ? HIGH : LOW);
  float pFVutil = pvOn ? pFV : 0.0f;                // anti-ilhamento
  float disp = fmaxf(0.0f, (BAT_CAP_KWH * (soc - SOC_MIN) / 100.0f) / DT_H);
  float pvToLoad = fminf(pFVutil, total);
  float rem = total - pvToLoad, batDis = 0, gridImp = 0;
  if (ponta) {
    batDis  = fminf(rem, fminf(BAT_MAX_KW, disp));   rem -= batDis;
    gridImp = fminf(rem, GRID_LIMIT_KW);             rem -= gridImp;
  } else {
    gridImp = fminf(rem, GRID_LIMIT_KW);             rem -= gridImp;
    batDis  = fminf(rem, fminf(BAT_MAX_KW, disp));   rem -= batDis;
  }
  float entregue = total - rem;                      // rem > 0 => potência reduzida (gestão de carga)
  float frac = (total > 0.01f) ? entregue / total : 0.0f;

  float pvSurplus = pFVutil - pvToLoad;
  float espaco = BAT_CAP_KWH * (100.0f - soc) / 100.0f / DT_H;
  float batChg = fminf(pvSurplus, fminf(BAT_MAX_KW, espaco));
  float exportKw = pvSurplus - batChg;
  float pBat  = batChg - batDis;                     // >0 carrega, <0 descarrega
  float pRede = exportKw - gridImp;                  // >0 exporta, <0 importa

  const char* modo;
  if (desarmado)               modo = "PROTECAO_ATIVA";
  else if (total < 0.01f)      modo = (batChg > 0.01f) ? "OCIOSO_CARREGA_BATERIA" : (exportKw > 0.01f ? "OCIOSO_EXPORTA" : "OCIOSO");
  else if (rem > 0.01f)        modo = "LIMITANDO_POTENCIA";
  else if (gridImp > 0.01f)    modo = "COMPLEMENTA_COM_REDE";
  else if (batDis > 0.01f)     modo = "USA_BATERIA";
  else                         modo = "SOL_DIRETO";

  // 5) Atualiza baterias e acumuladores
  soc = constrain(soc + pBat * DT_H / BAT_CAP_KWH * 100.0f, 0.0f, 100.0f);
  for (int i = 0; i < 2; i++) {
    cKw[i] = req[i] * frac;
    evSoc[i] = constrain(evSoc[i] + cKw[i] * DT_H / EV_CAP_KWH * 100.0f, 0.0f, 100.0f);
    digitalWrite(PIN_LED[i], cKw[i] > 0.1f ? HIGH : LOW);
  }
  float limpa = pvToLoad + batDis;                   // energia solar (direta ou via bateria)
  eVeic  += entregue * DT_H;
  eLimpa += limpa * DT_H;
  economia += limpa * DT_H * (ponta ? TARIFA_P : TARIFA_FP);
  picoKw = fmaxf(picoKw, gridImp);
  float pctLimpa = (eVeic > 0.01f) ? eLimpa / eVeic * 100.0f : 0.0f;
  float co2 = eLimpa * FATOR_CO2;
  float km = eVeic * KM_POR_KWH;

  horaSim += DT_H; if (horaSim >= 24) horaSim -= 24;

  // 6) MQTT (JSON para o dashboard)
  char json[768];
  snprintf(json, sizeof(json),
    "{\"hora\":%.2f,\"v_rede\":%.1f,\"pv_kw\":%.1f,\"pv_on\":%d,\"soc\":%.1f,\"bat_kw\":%.1f,\"rede_kw\":%.1f,"
    "\"rele\":\"%s\",\"modo\":\"%s\",\"ponta\":%d,"
    "\"c1_kw\":%.1f,\"c2_kw\":%.1f,\"c1_on\":%d,\"c2_on\":%d,\"c1_soc\":%.0f,\"c2_soc\":%.0f,"
    "\"req_kw\":%.1f,\"ent_kw\":%.1f,\"lim_kw\":%.0f,"
    "\"kwh_veic\":%.1f,\"kwh_limpa\":%.1f,\"pct_limpa\":%.0f,\"km\":%.0f,"
    "\"co2_kg\":%.2f,\"economia_rs\":%.2f,\"pico_kw\":%.1f,\"sessoes\":%d}",
    horaSim, vRede, pFVutil, pvOn ? 1 : 0, soc, pBat, pRede,
    desarmado ? "ABERTO" : "FECHADO", modo, ponta ? 1 : 0,
    cKw[0], cKw[1], plugado[0] ? 1 : 0, plugado[1] ? 1 : 0, evSoc[0], evSoc[1],
    total, entregue, GRID_LIMIT_KW,
    eVeic, eLimpa, pctLimpa, km, co2, economia, picoKw, sessoes);
  if (mqtt.connected()) mqtt.publish(TOPICO, json);

  // 7) Painel legível no terminal
  int h = (int)horaSim;
  int m = (int)((horaSim - h) * 60);
  Serial.printf("[%02d:%02d] %s | REDE %.0f V | SOL %.1f kW (%s) | BATERIA %.0f%%\n",
                h, m, ponta ? "PONTA" : "fora de ponta", vRede, pFV, pvOn ? "paineis ligados" : "PAINEIS DESCONECTADOS", soc);
  for (int i = 0; i < 2; i++) {
    if (plugado[i]) Serial.printf("        Carregador %d: %.1f kW | veiculo %.0f%%\n", i + 1, cKw[i], evSoc[i]);
    else            Serial.printf("        Carregador %d: livre\n", i + 1);
  }
  Serial.printf("        Modo: %s | Rele: %s | Rede: %.1f kW (limite %.0f) | MQTT: %s\n",
                modo, desarmado ? "ABERTO" : "FECHADO", pRede, GRID_LIMIT_KW, mqtt.connected() ? "OK" : "offline");
  Serial.printf("        Entregue aos veiculos: %.1f kWh | Energia limpa: %.0f%% | CO2 evitado: %.2f kg\n",
                eVeic, pctLimpa, co2);
  if (desarmado)    Serial.println("        *** PROTECAO ATIVA: tensao fora da faixa, carregadores desligados ***");
  if (rem > 0.01f)  Serial.println("        *** GESTAO DE CARGA: potencia reduzida para respeitar o limite da rede ***");
  Serial.println("----------------------------------------------------------------");
}
