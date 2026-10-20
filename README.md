# Estação de Recarga Solar Inteligente para Veículos Elétricos

**Sprint 4 – Solução final integrada | Desafio Goodwe**

Simulação de uma estação de recarga **comercial** com dois carregadores de 22 kW, usina solar e bateria estacionária. Um ESP32 decide de onde vem cada kWh (sol, bateria ou rede), protege a instalação contra tensão fora da faixa e envia a telemetria por MQTT para um painel em tempo real. O projeto usa como referência produtos e conceitos da linha Goodwe (inversor híbrido, bateria, carregador HCA e monitoramento no estilo SEMS Portal).

## Equipe

| Nome | RM |
|---|---|
| [Nome 1] | [RM] |
| [Nome 2] | [RM] |
| [Nome 3] | [RM] |
| [Nome 4] | [RM] |

## Links

- **Vídeo técnico (YouTube, não listado):** [colar link]
- **Projeto no Wokwi:** [colar link do projeto salvo]
- **Repositório:** [colar link do GitHub]

---

## 1. Visão geral

### Problema
Estações de recarga comerciais enfrentam três desafios: o **custo de demanda** (a rede cobra por pico de potência), o **horário de ponta** (energia mais cara) e a necessidade de **reduzir emissões** alimentando os veículos com energia limpa.

### Solução
Uma estação que combina geração solar, bateria e rede, com um algoritmo de despacho que:

1. **Prioriza o sol** para abastecer os veículos.
2. **Usa a bateria** para segurar picos e, na ponta, antes da rede.
3. **Respeita a demanda contratada** (limite de importação da rede) por meio de **gestão dinâmica de carga**.
4. **Protege a instalação**: desliga carregadores e painéis se a tensão da rede sair da faixa segura, e desconecta os painéis quando não há geração.
5. **Monitora tudo em tempo real** em um painel web, com indicadores de energia limpa, CO₂ evitado, economia e pico de demanda.

### Escopo da validação
A validação foi feita **em simulação** (Wokwi + painel web). O protótipo físico está em desenvolvimento e não foi usado nos resultados deste documento.

---

## 2. Arquitetura

```mermaid
flowchart LR
    subgraph Wokwi["Simulação no Wokwi (ESP32)"]
        P1["Pot 1: tensão da rede"]
        P2["Pot 2: nível de sol / nuvens"]
        S["Chaves 1 e 2: veículos plugados<br/>Chave 3: forçar ponta"]
        ESP["ESP32<br/>proteção + despacho + gestão de carga"]
        R1["Relé 1: proteção da rede"]
        R2["Relé 2: contator dos painéis"]
        L["LEDs: carregadores ativos"]
        P1 --> ESP
        P2 --> ESP
        S --> ESP
        ESP --> R1
        ESP --> R2
        ESP --> L
    end
    ESP -- "Wi-Fi + MQTT (JSON)" --> B[("Broker MQTT<br/>HiveMQ")]
    B -- "WebSocket" --> D["Painel web<br/>(dashboard.html)"]
```

### Camadas

| Camada | Componentes | Função |
|---|---|---|
| **Sensoriamento (simulado)** | 2 potenciômetros em ADC (pinos 35 e 34), 3 chaves (pinos 27, 14 e 13) | Tensão da rede, nível de sol, veículos plugados e ponta |
| **Processamento** | ESP32, código em C++ (Arduino) | Proteção, despacho de energia, gestão de carga, métricas |
| **Atuação** | Relé da rede (pino 26), relé dos painéis (pino 25), LEDs (pinos 18 e 19) | Desconectar, cortar painéis e sinalizar carregamento |
| **Comunicação** | Wi-Fi, MQTT (biblioteca PubSubClient), JSON | Telemetria a cada segundo |
| **Visualização** | `dashboard.html` (HTML, CSS, JavaScript, MQTT.js) | Fluxo de energia animado, gráfico, decisão em texto e indicadores |

### Escala simulada

| Parâmetro | Valor |
|---|---|
| Carregadores | 2 × 22 kW |
| Usina solar | 30 kW |
| Bateria estacionária | 30 kWh, carga e descarga até 15 kW, mínimo de 10% |
| Bateria do veículo | 60 kWh (carregamento reduz após 80%) |
| Demanda contratada (importação da rede) | 30 kW |
| Faixa segura de tensão | 176 a 242 V (80% a 110% de 220 V), com histerese (religa entre 187 e 233 V) |
| Corte dos painéis | Abre abaixo de 0,5 kW e reconecta acima de 1,0 kW |
| Tempo simulado | 1 s real = 15 min simulados (um dia em 96 s) |

---

## 3. Alinhamento ao desafio Goodwe

O projeto **não usa equipamentos Goodwe reais**. Ele **simula o comportamento** de uma estação construída com produtos e funções da marca, para demonstrar a lógica e a viabilidade da integração.

| O que foi simulado | Referência Goodwe | Observação |
|---|---|---|
| Carregador de 22 kW | Linha de carregadores HCA, com modelo de 22 kW (GW22K-HCA) | Fonte: distribuidores; confirmar no site oficial |
| Prioridade solar nos carregadores (`SOL_DIRETO`) | Carga por excedente solar (*surplus charging*) dos carregadores HCA em conexão com inversores Goodwe | |
| Gestão de carga e limite da rede | Recursos de gestão de carga e excedente solar da geração HCA-G2 | |
| Carga só com sol ou reforço pela rede | Modos de carga configuráveis no app SEMS+, com comunicação entre carregador, inversor e bateria | |
| Painel em tempo real | Acompanhamento das cargas no SEMS Portal e no app | O painel imita o conceito; não usa a API real |
| Proteções (relé da rede, anti-ilhamento) | Proteções integradas do carregador (corrente residual DC, parada de emergência) e do inversor | Confirmar proteções do inversor escolhido no datasheet |
| Inversor híbrido e bateria | Linha híbrida (solar + bateria + rede) | Modelo a confirmar no datasheet |
| Evolução para equipamento real | Comunicação do carregador G2 por RS485, LAN ou Wi-Fi (Modbus) | Próximo passo, não implementado |

### Alinhamento à disciplina
[Preencher com os conceitos vistos nas aulas. Sugestões: leitura de sensores analógicos (ADC), atuadores e relés, conectividade Wi-Fi, protocolo MQTT (publicação e assinatura), formato JSON, dashboards de monitoramento, simulação de hardware, automação baseada em regras com histerese.]

---

## 4. Lógica de decisão

### Ordem de prioridade

| Situação | Ordem de abastecimento dos veículos |
|---|---|
| **Fora de ponta** | Sol → Rede (até a demanda contratada) → Bateria |
| **Horário de ponta (18h às 21h)** | Sol → Bateria → Rede |
| **Sobra solar** | Bateria (até o limite de carga) → Exportação para a rede |
| **Pedido maior que o disponível** | Gestão de carga: reduz a potência dos carregadores proporcionalmente (`LIMITANDO_POTENCIA`) |

### Proteções
- **Tensão fora da faixa segura:** o relé da rede abre, os carregadores desligam e os painéis são desconectados (anti-ilhamento). Religa com histerese.
- **Sem geração solar:** o contator dos painéis abre, e bateria e rede assumem.
- **Carregamento realista:** o pedido de cada veículo cai quando a bateria passa de 80%.

### Modos reportados
`SOL_DIRETO`, `COMPLEMENTA_COM_REDE`, `USA_BATERIA`, `LIMITANDO_POTENCIA`, `OCIOSO_CARREGA_BATERIA`, `OCIOSO_EXPORTA`, `OCIOSO`, `PROTECAO_ATIVA`.

### Curva solar
A geração é o **nível de sol** (potenciômetro, 0 a 100%) multiplicado por uma curva senoidal do dia (zero antes das 6h e depois das 18h, pico ao meio-dia).

---

## 5. Como executar

1. **Simulação:** crie um projeto ESP32 no [Wokwi](https://wokwi.com) e cole `diagram.json`, `sketch_5.ino` (na aba `sketch.ino`, nome obrigatório no Wokwi) e `libraries.txt`. Rode com **Start**.
2. **Painel:** abra `dashboard.html` no navegador (arquivo local, com internet) com a simulação rodando.
3. **Teste sem o Wokwi:** no rodapé do painel, o botão **Simular dados** gera um dia completo com a mesma lógica.
4. **Tópico MQTT:** a constante `TOPICO` no sketch e no painel precisa ser **igual**. Use um nome exclusivo do grupo, porque o broker é público.

### Controles da simulação

| Controle | Função |
|---|---|
| Potenciômetro esquerdo | Tensão da rede (≈ 73% do giro = 220 V) |
| Potenciômetro direito | Nível de sol ou nuvens |
| Chaves 1 e 2 | Plugam veículo no carregador 1 e 2 |
| Chave 3 | Força o horário de ponta (opcional) |

---

## 6. Resultados

### Cenários testados

| Cenário | Resultado observado |
|---|---|
| Um veículo, sol alto | `SOL_DIRETO`: carregamento só com energia solar |
| Sem veículos, com sol | Sol carrega a bateria; bateria cheia, sobra exportada |
| Dois veículos, sol baixo | Rede e bateria completam; bateria segura os picos acima da demanda |
| Bateria no mínimo e pedido acima do limite | `LIMITANDO_POTENCIA`: gestão de carga reduz a potência |
| Noite | Painéis desconectados; rede e bateria abastecem os veículos |
| Tensão da rede fora de 176 a 242 V | Relé abre, carregadores e painéis desligam; religa com histerese |

### Resultados quantitativos (um dia simulado)

Preencher com os valores do painel após rodar um dia completo.

| Indicador | Valor |
|---|---|
| Energia entregue aos veículos | [__] kWh |
| Energia limpa (solar direta ou via bateria) | [__] % |
| Autonomia estimada entregue | [__] km |
| CO₂ evitado | [__] kg |
| Economia estimada | R$ [__] |
| Pico de demanda da rede | [__] kW (limite: 30 kW) |
| Sessões de recarga | [__] |

Inserir aqui: print do painel, print do circuito no Wokwi e print do terminal.

### Resultados qualitativos
- O painel torna a decisão do sistema legível (fluxo animado e frase de decisão).
- A gestão de carga evita ultrapassar a demanda contratada, protegendo o custo da instalação.
- A proteção com histerese evita desligamentos repetidos perto dos limites de tensão.

---

## 7. Avaliação crítica

### Sustentabilidade
- **Energia limpa:** priorizar o sol e armazenar a sobra na bateria aumenta o uso de energia renovável nos veículos.
- **Eficiência:** a bateria reduz picos de demanda e desloca consumo da ponta.
- **Emissões:** o CO₂ evitado é calculado com o fator médio da rede brasileira. Como esse fator é baixo, o benefício em CO₂ da energia solar é modesto. O ganho mais expressivo está em custo e gestão de demanda, e na troca de combustível fóssil por eletricidade.

### Inovação
Algoritmo de despacho com prioridades por horário, gestão dinâmica de carga, proteção com histerese, corte automático dos painéis, painel web em tempo real com decisão explicada e simulação completa de um dia.

### Limitações
- Validação feita **só em simulação**; o protótipo físico está em desenvolvimento.
- Entradas simuladas por potenciômetros e chaves, sem sensores reais de tensão ou corrente.
- Sem integração real com a API do SEMS Portal ou comunicação Modbus.
- Tarifas, limites, fator de emissão e autonomia são **premissas** (seção 8).
- Broker MQTT público, adequado para demonstração, mas não para produção.

### Próximos passos
Integração com inversor e carregador reais via Modbus, uso de sensores de tensão e corrente (por exemplo, módulos de medição), previsão de geração solar, tarifa dinâmica e opção de **zero injeção** (limitar a geração em vez de exportar).

---

## 8. Premissas e valores a confirmar

| Premissa | Valor usado | Observação |
|---|---|---|
| Fator de emissão da rede | 0,04 kg CO₂/kWh | Placeholder; usar o fator médio atualizado do MCTI |
| Tarifa fora de ponta | R$ 0,80/kWh | Ajustar para a distribuidora |
| Tarifa na ponta | R$ 2,00/kWh | Ajustar para a distribuidora |
| Horário de ponta | 18h às 21h | Confirmar na distribuidora |
| Demanda contratada | 30 kW | Premissa do cenário |
| Autonomia do veículo | 6 km/kWh | Estimativa |
| Limites de tensão | 80% a 110% de 220 V | Confirmar na norma de conexão (ABNT NBR 16149) e regra da distribuidora |
| Potências e capacidades | 22 kW, 30 kW, 30 kWh | Confirmar nos datasheets dos modelos Goodwe escolhidos |

---

## 9. Referências e ferramentas

### Ferramentas e tecnologias
- **Hardware simulado:** ESP32 DevKit, potenciômetros, chaves, relés e LEDs, no [Wokwi](https://wokwi.com)
- **Linguagem do firmware:** C++ (framework Arduino para ESP32)
- **Bibliotecas:** WiFi (ESP32), [PubSubClient](https://github.com/knolleary/pubsubclient) (MQTT)
- **Comunicação:** MQTT sobre Wi-Fi, broker público HiveMQ
- **Painel:** HTML, CSS, JavaScript, [MQTT.js](https://github.com/mqttjs/MQTT.js), fontes Bricolage Grotesque e IBM Plex Sans

### Referências Goodwe (consultadas em distribuidores; confirmar no site oficial da Goodwe)
- Carregador GW22K-HCA: https://shop.krannich-solar.com/en-se/EV-Charger-GW-22K-HCA/8253003
- Carregador GW7K-HCA G2 (gestão de carga, excedente solar, RS485, LAN e Wi-Fi): https://shop.krannich-solar.com/de-en/e-mobility/ac-charging-stations/71716/ev-charger-gw-7k-hca-g2
- Carregador GW22K-HCA-20 (modos no app SEMS+): https://www.solartraders.com/en/products/accessories/e-mobility/goodwe-gw22k-hca-20
- Site oficial: https://www.goodwe.com
- [Adicionar: datasheet do inversor híbrido e da bateria escolhidos]

### Outras referências
- [Adicionar: fator de emissão do MCTI, norma ABNT NBR 16149, tarifa da distribuidora]

---

## 10. Estrutura do repositório

```
.
├── README.md
├── wokwi/
│   ├── sketch_5.ino        (colar na aba sketch.ino do Wokwi)
│   ├── diagram.json
│   └── libraries.txt
├── dashboard/
│   └── dashboard.html
├── docs/
│   ├── prints/             (circuito, terminal, painel)
│   └── arquitetura.png     (opcional)
└── versao_residencial_v1/  (primeira versão, foco residencial)
```
