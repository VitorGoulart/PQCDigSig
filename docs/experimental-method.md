# Método experimental

## Unidade de execução

Uma *run* é definida por um algoritmo e um tamanho de carga fixos e contém
exatamente 1.000 iterações medidas. Cada iteração gera um novo par de chaves e
executa geração de chave, assinatura e verificação nos dois dispositivos.
Chaves e assinaturas nunca são reutilizadas.

Os 21 cenários são a combinação de sete algoritmos com cargas de 10 KiB,
100 KiB e 1 MiB. A matriz completa é repetida três vezes de forma independente.
A ordem das *runs* deve ser randomizada e registrada.

## Aquecimento

Antes de cada *run*, cada lado executa 20 iterações de aquecimento com o mesmo
algoritmo e tamanho. Elas exercitam o fluxo criptográfico, mas não são enviadas
como iterações medidas e não entram no CSV. O aquecimento reinicia sempre que o
algoritmo ou o tamanho muda.

## Cargas

As cargas são binárias e pseudoaleatórias, geradas fora das regiões
temporizadas. Em cada repetição, todos os algoritmos recebem exatamente os
mesmos bytes para um dado tamanho. Uma semente registrada em hexadecimal no
JSON permite reproduzi-las. A implementação deriva uma semente específica para
cada combinação de experimento, repetição e tamanho.

A construção implementada usa uma semente mestre de exatamente 32 bytes,
registrada como 64 dígitos hexadecimais. Para cada cenário, deriva-se:

```text
scenario_seed = SHA-256(master_seed || experiment_id_be32 ||
                       repetition_be32 || payload_size_be32)
payload = SHAKE-256("TCC-PAYLOAD-v1" || scenario_seed, payload_size)
```

Os inteiros são sem sinal e codificados em *big-endian*. A cadeia ASCII fixa
oferece separação de domínio e o sufixo `v1` congela esta versão do gerador. O
algoritmo não recebe o ID da assinatura: dentro da mesma repetição, todos os
algoritmos usam necessariamente a mesma carga para a combinação de experimento
e tamanho. Somente os tamanhos normativos de 10 KiB, 100 KiB e 1 MiB são
aceitos.

## Temporização

Usa-se relógio monotônico de alta resolução. A duração é armazenada como número
inteiro de nanossegundos. Serialização, rede, alocação da carga, gravação em
disco e atualização de interface ficam fora das regiões temporizadas.

Cada intervalo cobre a chamada pública completa do adaptador criptográfico. Em
`generate_key_pair`, isso inclui a geração pelo provedor e a exportação das
chaves para a representação binária compartilhada. Em `sign` e `verify`, inclui
a importação da chave binária, a criação/configuração do contexto do provedor e
a operação de assinatura ou verificação. Essa escolha mede o custo efetivamente
observado pelo núcleo comum e deve ser mantida idêntica nos dois dispositivos.
"Serialização" no parágrafo anterior significa a codificação do protocolo e do
CSV/JSON; a conversão de chave feita internamente pelo adaptador faz parte da
operação medida.

A carga e o conteúdo canônico assinado são preparados antes de iniciar o
cronômetro. A medição termina antes da construção da mensagem de transporte. A
recepção é concluída e o conteúdo canônico de entrada é reconstruído antes da
medição de verificação. Assim, enquadramento, cópias do protocolo e I/O TCP não
contaminam as três métricas.

## Falhas

Toda assinatura é verificada. Assinatura inválida ou erro criptográfico aborta
a *run*. Erro ou desconexão de rede aborta a conexão. Não há repetição
automática e uma tentativa refeita recebe novo `run_id`. Amostras da *run*
abortada não entram no conjunto válido nem são substituídas silenciosamente;
o JSON registra estado, código, iteração e detalhe da falha, e arquivos parciais
são preservados para diagnóstico.

## RSASSA-PSS

A configuração é fixa: chave RSA de 3.072 bits, expoente público 65.537,
SHA-256 como resumo, MGF1 com SHA-256, sal de exatamente 32 bytes, padding PSS e
chave pública em DER `SubjectPublicKeyInfo`. Todos esses parâmetros devem ser
configurados explicitamente pela API EVP do OpenSSL.

## Resultados CSV

O CSV contém somente o cabeçalho abaixo:

```csv
experiment_id,run_id,iteration,side,algorithm,payload_bytes,operation,duration
```

- `experiment_id`: inteiro sequencial positivo;
- `run_id`: inteiro sequencial dentro do experimento;
- `iteration`: 0 a 999;
- `side`: `client` ou `server`;
- `algorithm`: nome canônico do catálogo (`ML-DSA-44`, por exemplo);
- `payload_bytes`: 10.240, 102.400 ou 1.048.576;
- `operation`: `keygen`, `sign` ou `verify`;
- `duration`: inteiro de nanossegundos.

Uma iteração válida produz seis linhas. O CSV contém somente medições válidas;
provedor, versões, parâmetros e falhas pertencem ao JSON de metadados.

## Metadados JSON

O JSON possui `schema_version`, `experiment_id`, data UTC, configuração,
semente da carga, ordem das *runs*, versões das ferramentas e bibliotecas,
informações de cliente/servidor e uma lista de *runs*. Cada item registra
`run_id`, algoritmo, provedor, tamanho, repetição, estado (`completed` ou
`aborted`) e, quando aplicável, `error_code`, `failed_iteration` e
`error_detail`.

Estrutura normativa, com strings vazias preenchidas durante a coleta:

```json
{
  "schema_version": 1,
  "experiment_id": 1,
  "created_at_utc": "2026-08-26T18:30:00Z",
  "configuration": {
    "warmup_iterations": 20,
    "measured_iterations_per_run": 1000,
    "independent_repetitions": 3,
    "payload_sizes_bytes": [10240, 102400, 1048576],
    "payload_seed_hex": "",
    "run_order": "randomized"
  },
  "tool_versions": {
    "cmake_host": "4.2.3",
    "cmake_android": "4.1.2",
    "ninja": "1.13.2",
    "python": "3.14.4",
    "jdk_runtime": "25.0.2",
    "java_target": "17",
    "android_gradle_plugin": "9.2.0",
    "gradle": "9.4.1",
    "openssl": "3.5.7",
    "liboqs": "0.16.0"
  },
  "client": {
    "platform": "Android",
    "device_model": "",
    "android_version": "",
    "abi": "arm64-v8a",
    "ndk": "30.0.15729638",
    "clang": "21.0.0"
  },
  "server": {
    "platform": "Linux",
    "cpu": "AMD Ryzen 7 7800X3D",
    "os": "",
    "kernel": "",
    "clang": "21.1.8"
  },
  "runs": [
    {
      "run_id": 1,
      "repetition": 1,
      "algorithm": "ML-DSA-44",
      "provider": "OpenSSL",
      "payload_bytes": 10240,
      "status": "completed",
      "error_code": 0,
      "failed_iteration": null,
      "error_detail": ""
    }
  ]
}
```

Não são permitidos campos de duração no JSON: o CSV é a fonte das medições.
Datas usam UTC no formato RFC 3339. Falhas usam os códigos de `ERROR` definidos
em `docs/protocol.md`; `error_code` é zero em uma *run* concluída.

Cliente e servidor usam o mesmo `experiment_id`, recebido em `START_RUN`, e o
mesmo `run_id` e `iteration`. A consolidação correlaciona os arquivos por
`experiment_id + run_id + iteration`; `side` e `operation` distinguem as seis
medições.

## Implementação dos gravadores

O gravador CSV cria um arquivo novo, escreve exatamente o cabeçalho normativo e
valida identificadores, iteração, lado, algoritmo, carga, operação e duração
antes de cada linha. Números usam a localidade clássica e as linhas terminam em
LF. Escrita e `flush` com falha produzem erro tipado; o contador de linhas só é
incrementado depois de uma gravação bem-sucedida.

O serializador JSON valida o esquema fixo, a semente, a matriz de cargas, a
ordem randomizada, IDs únicos de *run*, repetição, provedor do algoritmo e os
campos de falha. Uma *run* `completed` não pode conter erro; uma *run* `aborted`
deve conter um código do protocolo. Strings são escapadas de acordo com JSON e
nenhuma duração é aceita pelo modelo de metadados. A implementação também
fornece horário UTC RFC 3339 e coleta, a partir do binário, versões do OpenSSL,
liboqs e compilador; as demais informações de ambiente continuam sendo
preenchidas pelas aplicações de cada dispositivo.
