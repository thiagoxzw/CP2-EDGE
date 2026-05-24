# 🍷 Nextstage (Vinheria Agnello) — Sistema de Monitoramento Ambiental

Projeto desenvolvido para a FIAP. O sistema monitora **temperatura, umidade e luminosidade** do ambiente de armazenamento de vinhos, fatores que influenciam diretamente a qualidade da bebida, e sinaliza condições fora do ideal por meio de LEDs, buzzer e display.

## 📌 Sobre o projeto

O vinho é sensível ao ambiente em que é guardado. Com base nos fatores de qualidade:

- **Temperatura:** ideal próxima de **13 °C**, com variação de no máximo ~3 °C. Calor excessivo e flutuações térmicas geram aromas indesejados.
- **Umidade:** ideal próxima de **70%**, na faixa de **60% a 80%**. Pouca umidade resseca a vedação; umidade demais danifica rótulos e favorece fungos.
- **Luminosidade:** ambiente em **penumbra**. A luz, em especial raios ultravioleta, altera os compostos do vinho.

O sistema lê esses três parâmetros em tempo real, exibe no display, alerta quando algo sai do ideal e registra logs das condições críticas.

## 🔧 Componentes

| Componente | Função |
|---|---|
| Arduino Uno | Controlador principal |
| Sensor DHT22 | Temperatura e umidade |
| Sensor LDR | Luminosidade |
| Display LCD 16x2 (I2C) | Exibição das informações |
| RTC DS1307 | Data e hora dos logs |
| EEPROM (interna) | Armazenamento de logs e configurações |
| LED verde / amarelo / vermelho | Sinalização de status |
| Buzzer | Alerta sonoro em estado crítico |
| 3 botões (Menu / Up / Down) | Navegação e ajuste do setup |

## 🔌 Pinagem

| Pino Arduino | Ligado a |
|---|---|
| D2 | DHT22 (dados) |
| A0 | LDR |
| D3 | LED verde |
| D4 | LED amarelo |
| D5 | LED vermelho |
| D6 | Buzzer |
| D7 | Botão Menu |
| D8 | Botão Up |
| D9 | Botão Down |
| A4 / A5 (SDA/SCL) | LCD I2C e RTC |

## ▶️ Como funciona

1. **Boot:** exibe o logo da Vinheria com animação e calibra o LDR automaticamente por 5 segundos.
2. **Operação:** o LCD alterna a cada 3 segundos entre três telas — data/hora (RTC), temperatura/umidade (DHT22) e luminosidade (%).
3. **Leitura de luz:** usa média móvel de 10 segundos, função `map()` e calibração automática para uma medição estável.
4. **Logs:** quando há condição crítica e o minuto muda, grava na EEPROM data, hora, temperatura e umidade.

## 🚦 Níveis de alerta

| LEDs | Buzzer | Significado |
|---|---|---|
| 🟢 Verde | — | Tudo dentro do ideal |
| 🟡 Amarelo | — | **1** fator fora do ideal |
| 🔴 Vermelho | Liga | **2 ou mais** fatores fora do ideal |

## ⚙️ Menu de setup

Pressione **Menu** para entrar. Dentro do menu:

- **Menu:** avança para o próximo item.
- **Up / Down:** ajustam o valor do item atual.

Itens, em ordem: Unidade (°C/°F) → Idioma (PT/EN) → Temp mín → Temp máx → Umid mín → Umid máx → Luz mín → Luz máx → **Salvar e sair?**

Para **sair**, avance até a tela "Salvar e sair?" e pressione **Menu** mais uma vez. As configurações são salvas na EEPROM e persistem mesmo após desligar.

## 🖥️ Simulação

Simule no [Wokwi](https://wokwi.com/projects/463742348394299393), que disponibiliza o sensor DHT22.

## 📚 Bibliotecas necessárias

- `LiquidCrystal_I2C`
- `RTClib`
- `Wire` (nativa)
- `EEPROM` (nativa)
- `DHT sensor library` (Adafruit)

## 📁 Estrutura do repositório

```
.
├── vinheria_agnello.ino     # Código do Arduino
├── README.md                # Este arquivo
├── circuito.png             # Imagem do circuito
└── docs/
    ├── encapsulamento.md    # Proposta de encapsulamento do protótipo
    └── valores_demonstracao.md  # Valores ideais para o vídeo
```

## 👥 Autores

João Vitor Batista de Mattos 
Thiago Souza de Lima
Matheus Akira Aso
