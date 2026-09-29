# Lead pack — Waveshare ESP32-S3-Touch-LCD-2.1

**Fonte:** https://docs.waveshare.com/ESP32-S3-Touch-LCD-2.1 · wiki Waveshare · schematic + 尺寸图 oficiais  
**Coletado:** 2026-09-29 · Groku CEO  
**SKU:** 28169 (flat) · 30697 = **2.1B** (vidro 2.5D curvado) — mesma eletrônica, CG diferente

## Resumo
Placa circular HMI: ESP32-S3R8 + LCD IPS **redondo 2.1"** **480×480** RGB (ST7701) + touch capacitivo CST820 (I2C), Wi‑Fi 2.4 / BLE5, antena onboard (IPEX1 via resoldagem). LVGL ok. **Não** tem speaker/mic (só buzzer).

## MCU / memória
| Item | Valor |
|---|---|
| SoC | ESP32-S3R8, Xtensa LX7 dual-core até 240 MHz |
| SRAM / ROM | 512 KB / 384 KB |
| PSRAM | **8 MB** |
| Flash | **16 MB** |

## Display / touch
| Item | Valor |
|---|---|
| Painel | IPS LCD 2.1" |
| Resolução | **480 × 480** |
| Cores | 262K no painel; stack ESP RGB565 → ~65K efetivos |
| Interface LCD | **RGB** (paralelo) + init SPI (GPIO1/2) |
| Driver LCD | **ST7701** |
| Touch | Capacitivo single-point **CST820**, I2C |
| BL | GPIO6 |

## Conectores (onboard)
1. **USB Type-C** (nativo USB / GPIO19–20) — dados/USB device  
2. **USB TO UART Type-C** — CH343P + auto-download (flash/debug/power)  
3. **SH1.0 12PIN** multi-função  
4. **SH1.0 4PIN I2C** (só I2C; compartilhado com chips onboard — **não remapear**)  
5. **SH1.0 4PIN UART** — **desabilitado** se o USB-UART Type-C estiver plugado (FSUSB42UMX)  
6. **MX1.25 2PIN** bateria Li‑ion 3.7 V (carga/descarga)  
7. Header **bateria RTC** recarregável  
8. Slot **microSD (TF)**  
9. **IPEX1** antena externa (precisa ressoldar resistor)  
10. Botões **RESET** / **BOOT** · switch alimentação bateria · LEDs power/carga · **buzzer**

Expansor **TCA9554PWR**: todos os EXIO usados onboard — **não trazidos para fora**.

## Header 12PIN (SH1.0)
| # | Label | Função |
|---|---|---|
| 1 | GND | GND |
| 2 | VBus | 5 V (USB) |
| 3 | D- | USB / **GPIO19** |
| 4 | D+ | USB / **GPIO20** |
| 5 | GND | GND |
| 6 | 3V3 | 3.3 V out |
| 7 | SCL | **GPIO7** I2C (não usar como GPIO genérico) |
| 8 | SDA | **GPIO15** I2C (idem) |
| 9 | TXD | **GPIO43** UART TX ou GPIO |
| 10 | RXD | **GPIO44** UART RX ou GPIO |
| 11 | NC | — |
| 12 | IO0 | **GPIO0** spare |

I2C 4PIN: GND · 3V3 · SCL(GPIO7) · SDA(GPIO15)  
UART 4PIN: GND · 3V3 · TXD(43) · RXD(44)

## GPIOs livres vs ocupados (importante pro case/periféricos)
**Quase todo GPIO útil do S3 está no LCD RGB / TF / I2C.** Disponíveis de verdade no header:
- **GPIO0** (IO0)
- **GPIO43 / GPIO44** (UART, se não usar o header UART / cuidado com USB-UART)
- **GPIO19 / GPIO20** (só se não usar USB nativo)

I2C bus interno compartilha GPIO7/15 — **não liberar**. Endereços I2C ocupados (FAQ): **0x15, 0x20, 0x51, 0x6B, 0x7E**.

### Mapa interno (não expor)
**LCD RGB:** BL=GPIO6 · RST=EXIO1 · SDA=GPIO1 · SCL=GPIO2 · CS=EXIO3 · PCLK=41 · DE=40 · VSYNC=39 · HSYNC=38 · B1–5=5/45/48/47/21 · G0–5=14/13/12/11/10/9 · R1–5=46/3/8/18/17  
**Touch:** SDA=15 · SCL=7 · INT=GPIO16 · RST=EXIO2  
**TF:** MISO=42 · MOSI=1 · SCLK=2 · CS=EXIO4 (MOSI/SCLK compartilhados c/ LCD init SPI)  
**IMU QMI8658:** I2C 7/15 · INT1=EXIO6 · INT2=EXIO5  
**RTC PCF85063:** I2C 7/15 · INT=EXIO7  
**Buzzer:** EXIO8  
**BAT_ADC:** GPIO4  

## Medidas físicas (oficial 尺寸图 · flat)
| Dimensão | mm |
|---|---|
| Diâmetro externo (vidro/óptica) | **Ø 75.00 ±0.15** |
| Área útil display (VA) | **Ø 53.88 ±0.15** |
| Diâmetro PCB | **Ø 65.00** |
| Espessura total (frente vidro → componentes) | **~9.50** |
| Vidro CG | **0.70 ±0.15** |
| Stack TP+LCD | **3.40** |
| Frente → plano PCB (aprox.) | **5.50** |
| Furos | **4× M2** |
| Furos superiores (centro a centro X) | **59.00** (±29.50 do eixo) |
| Furos inferiores (centro a centro X) | **36.00** (±18.00 do eixo) |

Arquivos locais: `refs/ESP32-S3-Touch-LCD-2.1 尺寸图.pdf`, `refs/dims-1.png`, `refs/ESP32-S3-Touch-LCD-2_1_asm.stp`, `refs/schematic.pdf`.

## Kit de caixa
Placa + cabo SH1.0 12PIN ~100 mm + 2× cabo SH1.0 4PIN ~100 mm.

## Dev
Arduino IDE ou ESP-IDF; demos LVGL na wiki. Dois Type-C: USB-UART (CH343) pra flash sem pular; USB nativo separado.

## Implicações pro próximo case (vs T-Display S3 anterior)
- Formato **circular Ø75**, não retangular.  
- Pouquíssimos GPIOs livres → periféricos externos preferir **I2C no barramento** (endereços livres) ou UART 43/44.  
- Dual USB-C + MX1.25 + TF no verso → envelope de case precisa prever saídas.  
- Variante **2.1B** = mesmo PCB, CG 2.5D — confirmar qual comprar antes do CAD.
