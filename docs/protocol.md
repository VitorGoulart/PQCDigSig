# Especificação do protocolo binário

## 1. Convenções

O protocolo usa uma conexão TCP exclusiva. Inteiros com mais de um byte são
sem sinal e codificados em *big-endian*. Índices de iteração começam em zero.
`experiment_id` e `run_id` são inteiros sequenciais, positivos e começam em 1.

Cada *run* contém exatamente 1.000 iterações medidas de um único algoritmo e
um único tamanho de carga. As 20 iterações de aquecimento são executadas
localmente antes da *run* e não trafegam como parte dela.

## 2. Cabeçalho comum

Toda mensagem começa com cinco bytes:

| Offset | Tamanho | Campo |
|---:|---:|---|
| 0 | 1 | `message_type: uint8` |
| 1 | 4 | `body_length: uint32` |

O receptor deve rejeitar tipos desconhecidos e `body_length` maior que
2.097.152 bytes antes de alocar o corpo. TCP não preserva mensagens; portanto,
o receptor deve ler exatamente 5 bytes e depois exatamente `body_length` bytes.

## 3. Identificadores

### 3.1 Algoritmos

| ID | Algoritmo | Provedor |
|---:|---|---|
| `0x01` | ML-DSA-44 | OpenSSL |
| `0x02` | ML-DSA-65 | OpenSSL |
| `0x03` | ML-DSA-87 | OpenSSL |
| `0x04` | Falcon-512 | liboqs |
| `0x05` | Falcon-1024 | liboqs |
| `0x06` | RSASSA-PSS | OpenSSL |
| `0x07` | Ed25519 | OpenSSL |

`0x00` e `0x08`–`0xFF` são inválidos.

### 3.2 Mensagens

| ID | Mensagem | Direção |
|---:|---|---|
| `0x01` | `START_RUN` | cliente → servidor |
| `0x02` | `START_RUN_ACK` | servidor → cliente |
| `0x03` | `SIGNED_REQUEST` | cliente → servidor |
| `0x04` | `SIGNED_RESPONSE` | servidor → cliente |
| `0x05` | `END_RUN` | cliente → servidor |
| `0x06` | `END_RUN_ACK` | servidor → cliente |
| `0x07` | `FINISH` | cliente → servidor |
| `0x08` | `ERROR` | ambas as direções |

Não existe `RUN_COMPLETE`: `END_RUN` e `END_RUN_ACK` encerram a *run* sem
duplicar essa função.

## 4. Corpos das mensagens

### 4.1 `START_RUN` (`body_length = 17`)

| Offset | Tamanho | Campo |
|---:|---:|---|
| 0 | 4 | `experiment_id: uint32` |
| 4 | 4 | `run_id: uint32` |
| 8 | 1 | `algorithm_id: uint8` |
| 9 | 4 | `payload_length: uint32` |
| 13 | 4 | `iteration_count: uint32` |

`iteration_count` deve ser 1.000. `payload_length` deve ser 10.240, 102.400 ou
1.048.576. Um `run_id` não pode ser reutilizado na mesma conexão.

### 4.2 `START_RUN_ACK` (`body_length = 5`)

| Offset | Tamanho | Campo |
|---:|---:|---|
| 0 | 4 | `run_id: uint32` |
| 4 | 1 | `status: uint8` |

Status: `0` aceito; `1` algoritmo inválido; `2` tamanho inválido; `3` número de
iterações inválido; `4` identificador inválido/repetido; `5` servidor ocupado.

### 4.3 `SIGNED_REQUEST` e `SIGNED_RESPONSE`

| Offset | Tamanho | Campo |
|---:|---:|---|
| 0 | 4 | `run_id: uint32` |
| 4 | 4 | `iteration: uint32` |
| 8 | 4 | `public_key_length: uint32` |
| 12 | 4 | `signature_length: uint32` |
| 16 | 4 | `payload_length: uint32` |
| 20 | variável | `public_key` |
| seguinte | variável | `signature` |
| seguinte | variável | `payload` |

Deve valer:

```text
body_length = 20 + public_key_length + signature_length + payload_length
```

Limites: chave pública até 16.384 bytes, assinatura até 16.384 bytes e carga
até 1.048.576 bytes. A carga deve ter o tamanho declarado em `START_RUN`, e
`iteration` deve corresponder exatamente à próxima iteração esperada.

### 4.4 `END_RUN` (`body_length = 8`)

| Offset | Tamanho | Campo |
|---:|---:|---|
| 0 | 4 | `run_id: uint32` |
| 4 | 4 | `completed_iterations: uint32` |

`completed_iterations` deve ser 1.000.

### 4.5 `END_RUN_ACK` (`body_length = 5`)

| Offset | Tamanho | Campo |
|---:|---:|---|
| 0 | 4 | `run_id: uint32` |
| 4 | 1 | `status: uint8` (`0` = encerrada; `1` = rejeitada) |

### 4.6 `FINISH` (`body_length = 0`)

Encerra a sessão de aplicação e, após o envio de todos os bytes pendentes, a
conexão TCP.

### 4.7 `ERROR`

| Offset | Tamanho | Campo |
|---:|---:|---|
| 0 | 2 | `error_code: uint16` |
| 2 | 1 | `related_message_type: uint8` |
| 3 | 4 | `run_id: uint32`, zero quando não aplicável |
| 7 | 4 | `iteration: uint32`, zero quando não aplicável |
| 11 | 2 | `description_length: uint16` |
| 13 | variável | descrição UTF-8 |

A descrição possui no máximo 256 bytes e serve somente para diagnóstico. A
lógica usa `error_code`. Códigos: `1` mensagem inesperada; `2` corpo inválido;
`3` limite excedido; `4` algoritmo inválido; `5` assinatura inválida; `6` erro
criptográfico; `7` identificador inválido; `8` erro interno.

## 5. Conteúdo assinado

Cliente e servidor assinam exatamente:

```text
message_type:uint8 || algorithm_id:uint8 || run_id:uint32 ||
iteration:uint32 || payload_length:uint32 || payload:bytes
```

Os inteiros seguem *big-endian*. Chave pública e assinatura não integram o
conteúdo assinado. Incluir `run_id` impede reutilização entre *runs* da mesma
conexão.

## 6. Máquina de estados

```text
CONNECTED → START_RUN → START_RUN_ACK
          → 1000 × (SIGNED_REQUEST → SIGNED_RESPONSE)
          → END_RUN → END_RUN_ACK
          → START_RUN (nova run) ou FINISH
```

Qualquer tipo, identificador ou iteração fora da ordem gera `ERROR`. Falha
criptográfica, assinatura inválida, truncamento ou desconexão aborta a *run*;
não há repetição automática nem retomada após reconexão.

## 7. Limites globais

| Item | Limite |
|---|---:|
| Corpo | 2.097.152 bytes |
| Carga | 1.048.576 bytes |
| Chave pública | 16.384 bytes |
| Assinatura | 16.384 bytes |
| Descrição de erro | 256 bytes |
| Iterações medidas por *run* | exatamente 1.000 |
| *Runs* por conexão | 1.000 |
