# Documentação do Simulador Tomasulo

Este documento descreve o **funcionamento do algoritmo de Tomasulo** implementado neste projeto e explica **como o código fonte está organizado**, com referência direta aos arquivos do repositório.

Referência bibliográfica: HENNESSY, John L.; PATTERSON, David A. *Arquitetura de Computadores: Uma Abordagem Quantitativa*.

---

## 1. O que é o algoritmo de Tomasulo?

O algoritmo de Tomasulo é uma técnica de **execução dinâmica de instruções** que permite:

- **Execução fora de ordem** (out-of-order), respeitando dependências de dados
- **Eliminação de hazards RAW** (Read After Write) via renomeação de registradores
- **Compartilhamento de resultados** pelo barramento CDB (Common Data Bus)

Em vez de esperar que um registrador seja atualizado no banco de registradores, instruções dependentes recebem o **nome da estação de reserva** que produzirá o valor (`Qj`, `Qk` ou `Qi`).

### Componentes principais do hardware simulado

```
┌─────────────────┐     ┌──────────────────────┐     ┌─────────────┐
│ Fila de         │────▶│ Estações de Reserva  │────▶│ Unidades    │
│ Instruções      │     │ (Load/Add/Mult)      │     │ Funcionais  │
└─────────────────┘     └──────────────────────┘     └──────┬──────┘
                                                            │
                         ┌──────────────────────┐          │
                         │ Banco de Registradores │◀─────────┤
                         │ FP (F0–F31)           │          │
                         └──────────────────────┘          │
                                    ▲                      │
                                    │         ┌────────────▼──┐
                                    └─────────│ CDB (broadcast)│
                                              └─────────────────┘
```

| Componente | Função |
|------------|--------|
| **Estações de reserva** | Buffer onde instruções aguardam operandos e executam |
| **Qi / Reorder #** | Indica qual estação (ou entrada ROB) atualizará o registrador |
| **CDB** | Propaga resultados prontos para estações e registradores |
| **ROB** (modo avançado) | Garante commit in-order após execução especulativa |

---

## 2. Modos de operação

### 2.1 Modo clássico (`ClassicTomasulo`)

Implementado em [`include/classic_tomasulo.hpp`](include/classic_tomasulo.hpp) e [`src/classic_tomasulo.cpp`](src/classic_tomasulo.cpp).

Utiliza três tabelas de estado, conforme os slides 104–108:

1. **Instruction Status** — estágio de cada instrução (Issue, Execute, Write Result)
2. **Reservation Stations** — conteúdo de Load1/2, Add1/2/3, Mult1/2
3. **Register Status** — campo `Qi` por registrador FP

Renomeação: quando uma instrução emite com destino `Fd`, o registrador `Fd` passa a apontar para a estação que produzirá o resultado (ex.: `Qi[F6] = Load1`).

### 2.2 Modo com ROB (`RobTomasulo`)

Implementado em [`include/rob_tomasulo.hpp`](include/rob_tomasulo.hpp) e [`src/rob_tomasulo.cpp`](src/rob_tomasulo.cpp).

Adiciona o **Reorder Buffer** para suportar especulação com **commit in-order** (slides 110+):

1. **Reorder Buffer** — fila circular de instruções emitidas
2. **Reservation Stations** — com coluna extra `Dest` (# da entrada ROB)
3. **FP Register Status** — `Reorder #` e `Busy` por registrador

Diferença-chave: operandos pendentes referenciam `#N` (entrada ROB) em vez do nome da estação. O commit só ocorre quando a entrada mais antiga do ROB está pronta.

---

## 3. Estágios de execução (por ciclo de clock)

Cada chamada a `step()` executa, **nesta ordem**:

```
1. Commit       → apenas modo ROB
2. Write Result → broadcast no CDB (1 resultado/ciclo)
3. Execute      → unidades funcionais decrementam latência
4. Issue        → emite próxima instrução da fila
5. (main)       → imprime tabelas de estado
```

### 3.1 Issue (emissão)

**Arquivo:** `issueStage()` em `classic_tomasulo.cpp` / `rob_tomasulo.cpp`

1. Verifica se há instrução pendente na fila
2. Seleciona estação livre do tipo correto:
   - `L.D` / `S.D` → Load1 ou Load2
   - `ADD.D` / `SUB.D` → Add1, Add2 ou Add3
   - `MUL.D` / `DIV.D` → Mult1 ou Mult2
3. Se não houver estação livre → **stall** (não emite neste ciclo)
4. Preenche operandos via `captureOperand()`:
   - Se registrador tem `Qi`/`Busy` → `Qj` ou `Qk` recebe a tag
   - Se valor já disponível → `Vj` ou `Vk` recebe snapshot numérico
5. Para loads: campo `A = offset + Regs[Rs]`
6. Atualiza `Qi[dest]` (clássico) ou aloca entrada ROB (modo ROB)

### 3.2 Execute (execução)

**Arquivo:** `executeStage()`

1. Para cada estação ocupada, verifica se `Qj` e `Qk` estão vazios (operandos prontos)
2. Se prontos e ainda não executando → inicia FU com latência configurada
3. Decrementa `cyclesLeft` a cada ciclo
4. Quando `cyclesLeft == 0` → marca `readyToWrite = true`

Latências padrão (configuráveis via `LAT` no arquivo de entrada):

| Operação | Ciclos |
|----------|--------|
| Load / Store | 1 |
| Add / Sub | 2 |
| Mul | 10 |
| Div | 40 |

### 3.3 Write Result (escrita de resultado)

**Arquivo:** `writeResultStage()` + `broadcastCdb()`

1. Seleciona **uma** estação com `readyToWrite` (CDB único, prioridade FIFO)
2. Calcula resultado via `computeResult()`
3. **Broadcast CDB:**
   - Percorre todas as estações: se `Qj`/`Qk` apontam para esta estação → copia valor e limpa tag
   - Atualiza `Qi[dest]` e banco de registradores (clássico)
   - Atualiza entrada ROB para estado `Write result` (modo ROB)
4. Libera estação (`busy = false`)

### 3.4 Commit (somente ROB)

**Arquivo:** `commitStage()` em `rob_tomasulo.cpp`

1. Examina entrada mais antiga do ROB (`robHead_`)
2. Se estado = `Write result` → commit in-order
3. Escreve valor no banco de registradores se `Reorder #` ainda aponta para esta entrada
4. Avança `robHead_` e marca instrução como `committed`

---

## 4. Estruturas de dados

Definidas em [`include/types.hpp`](include/types.hpp):

### Instruction
Representa uma instrução parseada do arquivo de entrada.

```cpp
struct Instruction {
    OpCode opcode;      // LD, SD, ADD_D, SUB_D, MUL_D, DIV_D
    std::string dest;   // registrador destino (Fd)
    std::string src1;   // primeiro operando (Fs)
    std::string src2;   // segundo operando (Ft)
    int offset;         // deslocamento para load/store
    std::string baseReg;// registrador base (Rs)
    std::string text;   // linha original
    int index;          // posição na fila
};
```

### ReservationStation
Modelo de cada estação de reserva (Load1, Add1, Mult1, etc.).

```cpp
struct ReservationStation {
    std::string name;       // "Load1", "Add1", "Mult1"...
    bool busy;              // estação ocupada?
    std::string op;         // Load, ADD, SUB, MUL, DIV, Store
    std::string vj, vk;     // valores dos operandos (se prontos)
    std::string qj, qk;     // tags das estações/ROB produtoras
    std::string a;          // endereço (loads/stores)
    int dest;               // # entrada ROB (modo ROB)
    int cyclesLeft;         // ciclos restantes de execução
    bool executing;         // FU em execução?
    bool readyToWrite;      // pronto para CDB?
    double resultValue;     // resultado numérico calculado
};
```

### RegisterStatusClassic / RegisterStatusRob

- **Clássico:** `qi` = nome da estação que escreverá no registrador (vazio = valor válido no banco)
- **ROB:** `reorderNum` + `busy` = entrada ROB responsável pelo registrador

### RobEntry (modo ROB)

```cpp
struct RobEntry {
    bool busy;
    int number;             // número sequencial (#1, #2, #3...)
    Instruction instruction;
    RobState state;         // Issue, Execute, WriteResult, Commit
    std::string destination;
    std::string value;      // representação simbólica do resultado
    double numericValue;    // valor numérico
    bool ready;             // resultado disponível?
};
```

---

## 5. Organização do código fonte

```
Trab_Ac_02/
├── include/
│   ├── types.hpp              # Structs e enums centrais
│   ├── parser.hpp             # Interface do parser de entrada
│   ├── display.hpp            # Interface de impressão das tabelas
│   ├── classic_tomasulo.hpp   # Classe do simulador clássico
│   └── rob_tomasulo.hpp       # Classe do simulador com ROB
├── src/
│   ├── main.cpp               # Ponto de entrada, CLI, loop principal
│   ├── parser.cpp             # Leitura e parse do arquivo .txt
│   ├── display.cpp            # Formatação das tabelas ciclo a ciclo
│   ├── classic_tomasulo.cpp   # Lógica Issue/Execute/WriteResult
│   └── rob_tomasulo.cpp       # Lógica + Commit com ROB
├── examples/
│   ├── classic_exemplo.txt    # Exemplo dos slides (modo clássico)
│   └── rob_exemplo.txt        # Exemplo dos slides (modo ROB)
├── Makefile
└── README.md
```

### 5.1 `main.cpp` — fluxo principal

```cpp
SimulationConfig config = parseInputFile(inputFile);

if (config.mode == SimMode::Classic) {
    ClassicTomasulo sim(config);
    printClassicState(out, sim, 0);       // estado inicial
    while (sim.step()) {                  // ciclo a ciclo
        printClassicState(out, sim, sim.getCycle());
    }
    printFinalRegisters(out, ...);         // resultado final
}
```

O loop `while (sim.step())` termina quando todas as instruções completaram Write Result (clássico) ou Commit (ROB).

### 5.2 `parser.cpp` — leitura de entrada

Função principal: `parseInputFile(filename)` → retorna `SimulationConfig`.

Reconhece linhas do tipo:
- `MODE classic|rob`
- `R2 100` → registrador inteiro
- `F4 3.0` → registrador FP
- `MEM 132 5.0` → célula de memória
- `LAT MUL 10` → latência customizada
- `L.D F6, 32(R2)` → instrução MIPS FP

Comentários (`#`) e linhas vazias são ignorados.

### 5.3 `display.cpp` — saída formatada

| Função | Descrição |
|--------|-----------|
| `printClassicState()` | Imprime 3 tabelas do modo clássico |
| `printRobState()` | Imprime 3 tabelas do modo ROB |
| `printFinalRegisters()` | Imprime registradores FP/int finais |

### 5.4 Funções-chave do simulador clássico

| Função | Papel |
|--------|-------|
| `initStations()` | Cria Load1/2, Add1/2/3, Mult1/2 |
| `findFreeStation(type)` | Busca estação livre por tipo |
| `captureOperand(reg, v, q)` | Resolve operando: valor ou tag Qi |
| `issueStage()` | Emite instrução na estação |
| `executeStage()` | Executa FU com latência |
| `writeResultStage()` | Seleciona estação para CDB |
| `broadcastCdb(rs)` | Propaga resultado e libera estação |
| `computeResult(rs)` | Calcula resultado aritmético/memória |
| `parseNumericOrMem(token)` | Interpreta Vj/Vk (número, Regs[], Mem[]) |

---

## 6. Exemplo passo a passo: `meu_teste.txt`

Arquivo de entrada:

```txt
MODE classic
R2 100
F4 3.0
MEM 132 5.0
L.D F6, 32(R2)
ADD.D F0, F6, F4
```

### Ciclo 0 (estado inicial)
Todas as estações livres. Nenhuma instrução emitida.

### Ciclo 1
- **Issue:** `L.D F6, 32(R2)` → Load1
  - `A = 32 + Regs[R2]`
  - `Qi[F6] = Load1`
- **Execute:** Load1 inicia (latência 1)
- Load1: `busy=Yes`, `op=Load`, `A=32 + Regs[R2]`

### Ciclo 2
- **Write Result:** Load1 completa → `F6 = Mem[132] = 5.0`
  - Broadcast: limpa `Qi[F6]`, libera Load1
- **Issue:** `ADD.D F0, F6, F4` → Add1
  - `Vj = 5.0` (F6 disponível), `Vk = 3.0` (F4 disponível)
  - `Qi[F0] = Add1`
- **Execute:** Add1 inicia (latência 2)

### Ciclos 3–4
- Add1 executando (`cyclesLeft` decrementando)

### Ciclo 5
- **Write Result:** Add1 completa → `F0 = 5.0 + 3.0 = 8.0`
  - Libera Add1, limpa `Qi[F0]`

### Resultado final
```
F6 = 5.0000
F0 = 8.0000
F4 = 3.0000
R2 = 100
```

---

## 7. Tratamento de dependências (hazards)

### RAW (Read After Write)
Resolvido por renomeação: se `Qi[F2] = Load2`, operandos que leem F2 recebem `Qj = Load2` e aguardam o broadcast do CDB.

### WAW / WAR (Write After Write / Write After Read)
Eliminados pela renomeação via `Qi` ou ROB: cada instrução escreve em uma "tag" intermediária, não diretamente no registrador arquitetural até o commit/write result.

### Estrutural (falta de estação)
Se todas as Load estiverem ocupadas e chegar outro `L.D`, a emissão **espera** (`issueStage` retorna sem emitir) até uma estação ser liberada.

### CDB (recurso único)
Apenas **uma** estação por ciclo pode fazer write result. Se duas terminam no mesmo ciclo, a segunda espera o ciclo seguinte.

---

## 8. Diferenças entre modo clássico e ROB

| Aspecto | Clássico | ROB |
|---------|----------|-----|
| Renomeação | `Qi` = nome da estação | `Reorder #` = número da entrada ROB |
| Operandos pendentes | `Qj/Qk` = "Load2", "Mult1" | `Qj/Qk` = "#2", "#3" |
| Atualização do banco | No Write Result | No Commit (in-order) |
| Tabela extra | — | Reorder Buffer |
| Condição de término | Todas com Write Result | Todas com Commit |

### Detalhe importante no modo ROB

Quando uma instrução emite **após** o write result de uma dependência mas **antes** do commit, o registrador ainda aparece como `Busy`. O código verifica se a entrada ROB já tem `ready = true` e, nesse caso, captura o valor diretamente em vez de criar dependência fantasma (que causaria deadlock).

Implementado em `captureOperandRob()` em `rob_tomasulo.cpp`.

---

## 9. Como compilar e testar

```bash
cd /caminho/para/Trab_Ac_02
make
./tomasulo meu_teste.txt
./tomasulo examples/classic_exemplo.txt
./tomasulo -m rob examples/rob_exemplo.txt
./tomasulo meu_teste.txt -o trace.txt    # salva trace em arquivo
```

---

## 10. Limitações conhecidas

- Emissão limitada a **1 instrução por ciclo**
- CDB único: **1 write-back por ciclo**
- Sem predição de desvios ou flush por misprediction
- Latências fixas por tipo de operação
- Memória modelada como mapa endereço → valor (sem hierarquia de cache)

---

## 11. Referências no código

Para estudo do algoritmo, recomenda-se ler nesta ordem:

1. [`include/types.hpp`](include/types.hpp) — entender as estruturas
2. [`src/parser.cpp`](src/parser.cpp) — como a entrada é lida
3. [`src/classic_tomasulo.cpp`](src/classic_tomasulo.cpp) — algoritmo completo (modo mais simples)
4. [`src/rob_tomasulo.cpp`](src/rob_tomasulo.cpp) — extensão com ROB e commit
5. [`src/display.cpp`](src/display.cpp) — formato de saída
6. [`src/main.cpp`](src/main.cpp) — integração e loop de simulação
