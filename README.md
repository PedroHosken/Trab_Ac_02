# Simulador do Algoritmo de Tomasulo

Simulador educacional em C++ do algoritmo de Tomasulo para ponto flutuante MIPS, baseado em *Arquitetura de Computadores: Uma Abordagem Quantitativa* (Hennessy & Patterson).

O simulador suporta dois modos:
- **Clássico** — estações de reserva com renomeação via `Qi` (slides 104–108)
- **Com ROB** — Reorder Buffer para execução especulativa com commit in-order (slides 110+)

## Requisitos

- Compilador C++17 (`g++` ou `clang++`)
- `make`

## Compilação

```bash
make
```

Gera o executável `tomasulo`.

## Uso

```bash
./tomasulo arquivo_entrada.txt
./tomasulo -m classic examples/classic_exemplo.txt
./tomasulo -m rob examples/rob_exemplo.txt
./tomasulo -o trace.txt examples/classic_exemplo.txt
```

### Opções

| Opção | Descrição |
|-------|-----------|
| `-m classic` | Força modo Tomasulo clássico |
| `-m rob` | Força modo com Reorder Buffer |
| `-o arquivo` | Salva o trace ciclo a ciclo em arquivo |
| `-h` | Exibe ajuda |

## Formato do arquivo de entrada

Arquivo texto com comentários (`#`) e as seguintes linhas:

```txt
MODE classic          # ou "rob"
R2 100                # registradores inteiros (base de endereço)
R3 200
F4 3.0                # registradores FP iniciais
MEM 132 5.0           # memória: endereço -> valor
MEM 244 2.0
L.D F6, 32(R2)        # instruções MIPS FP
L.D F2, 44(R3)
MUL.D F0, F2, F4
SUB.D F8, F2, F6
DIV.D F10, F0, F6
ADD.D F6, F8, F2
```

### Instruções suportadas

| Instrução | Descrição |
|-----------|-----------|
| `L.D Fd, offset(Rs)` | Load double de memória |
| `S.D Fs, offset(Rs)` | Store double em memória |
| `ADD.D Fd, Fs, Ft` | Soma ponto flutuante |
| `SUB.D Fd, Fs, Ft` | Subtração ponto flutuante |
| `MUL.D Fd, Fs, Ft` | Multiplicação ponto flutuante |
| `DIV.D Fd, Fs, Ft` | Divisão ponto flutuante |

Endereço efetivo: `offset + Regs[Rs]`.

### Latências configuráveis (opcional)

```txt
LAT LOAD 1
LAT STORE 1
LAT ADD 2
LAT MUL 10
LAT DIV 40
```

## Arquitetura simulada

| Recurso | Quantidade |
|---------|-----------|
| Load buffers | Load1, Load2 |
| Estações Add/Sub | Add1, Add2, Add3 |
| Estações Mult/Div | Mult1, Mult2 |
| Registradores FP | F0–F31 |
| Registradores int | R0–R31 |
| CDB | 1 resultado por ciclo |
| ROB | mínimo 16 entradas (modo ROB) |

## Saída

A cada ciclo de clock, o simulador imprime automaticamente:

**Modo clássico:**
1. Instruction Status (Issue / Execute / Write result)
2. Reservation Stations (Busy, Op, Vj, Vk, Qj, Qk, A)
3. Register Status (Qi por registrador)

**Modo ROB:**
1. Reorder Buffer (Entry, Busy, Instruction, State, Destination, Value)
2. Reservation Stations (com coluna Dest)
3. FP Register Status (Reorder #, Busy)

Ao final, exibe os valores dos registradores FP e inteiros utilizados.

## Lógica de simulação

Ordem das etapas em cada ciclo:

```
1. Commit      (somente modo ROB)
2. Write Result (broadcast no CDB)
3. Execute      (unidades funcionais)
4. Issue        (emissão da próxima instrução)
5. Impressão das tabelas de estado
```

### Issue
- Seleciona estação de reserva livre do tipo correto (Load/Add/Mult)
- Preenche operandos (`Vj`/`Vk` ou `Qj`/`Qk`) consultando o estado dos registradores
- Atualiza `Qi` (clássico) ou aloca entrada no ROB (modo ROB)
- Stall se não houver estação ou entrada ROB disponível

### Execute
- Inicia execução quando operandos estão prontos
- Decrementa contador de latência da unidade funcional
- Marca instrução como pronta para Write Result

### Write Result
- Seleciona uma estação pronta (CDB único, FIFO)
- Broadcast do resultado para estações dependentes e registradores
- Libera estação de reserva

### Commit (ROB)
- Commit in-order da entrada mais antiga do ROB quando em estado "Write result"
- Atualiza register file se nenhuma instrução mais recente renomeou o registrador

## Estrutura do código

```
include/
  types.hpp           — estruturas de dados centrais
  parser.hpp          — leitura do arquivo de entrada
  display.hpp         — formatação das tabelas
  classic_tomasulo.hpp — simulador clássico
  rob_tomasulo.hpp    — simulador com ROB
src/
  main.cpp            — CLI e loop principal
  parser.cpp
  display.cpp
  classic_tomasulo.cpp
  rob_tomasulo.cpp
examples/
  classic_exemplo.txt — sequência dos slides (modo clássico)
  rob_exemplo.txt     — sequência dos slides (modo ROB)
```

## Exemplo de execução

```bash
make run-classic
```

Resultado esperado (registradores finais):

| Registrador | Valor |
|-------------|-------|
| F0 | 6.0 (2.0 × 3.0) |
| F2 | 2.0 |
| F6 | -1.0 (F8 + F2 = -3 + 2) |
| F8 | -3.0 (F2 - F6 = 2 - 5) |
| F10 | 1.2 (F0 / F6 = 6 / 5) |

## Limitações

- Um único barramento CDB (1 write-back por ciclo)
- Latências fixas por tipo de operação
- Sem predição de desvios ou flush de especulação incorreta
- Emissão de no máximo 1 instrução por ciclo

## Referência

HENNESSY, John L.; PATTERSON, David A. *Arquitetura de Computadores: Uma Abordagem Quantitativa*.
