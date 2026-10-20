# Simulação no Wokwi

Arquivos do circuito e do firmware da estação de recarga.

| Arquivo | Onde colar no Wokwi |
|---|---|
| `diagram.json` | Aba **diagram.json** |
| `sketch_5.ino` | Aba **sketch.ino** (o Wokwi exige esse nome para o arquivo principal) |
| `libraries.txt` | Aba **libraries.txt** |

Passos:
1. Em wokwi.com, crie um projeto **ESP32**.
2. Cole o conteúdo de cada arquivo na aba indicada.
3. Salve (Save) e rode com **Start**.

Link do projeto salvo: [colar aqui]

Controles: potenciômetro esquerdo = tensão da rede (≈ 73% do giro = 220 V); direito = nível de sol; chaves 1 e 2 = veículos plugados; chave 3 = forçar horário de ponta.
