# Identificação do firmware

Feature Report 34, 24 bytes, disponível no USB HID e no HID Bluetooth da
guitarra. Leitura sem gravação na flash; SET não altera a identificação.

| Offset | Campo |
| --- | --- |
| 0–3 | ASCII XCFW |
| 4 | Versão do protocolo: 1 |
| 5–7 | Versão do firmware: major, minor, patch (0.1.0) |
| 8 | Papel: 0 guitarra, 1 receptor |
| 9 | Capacidades: bit 0 Bluetooth disponível, bit 1 XSM3 |
| 10 | Compilação com alterações locais: 0/1 |
| 11 | Reservado zero |
| 12–23 | Commit Git ASCII, 12 caracteres; unknown com preenchimento zero sem Git |

O commit e o indicador de alterações são obtidos ao configurar o CMake.
Execute scripts/build.ps1 para atualizar os metadados antes da compilação.
O papel vem do alvo compilado; Bluetooth depende da detecção física da placa.
O editor consulta na conexão. Firmware sem report 34 continua configurável e
mostra identificação indisponível. Alterações locais não são identificadas
apenas pelo hash: a interface também mostra “alterações locais”.
