# TCC — assinaturas digitais

Estrutura mínima para comparar sete configurações de assinatura digital entre
dois clientes Android e um servidor Linux.

- `clientAndroid16`: cliente executado no dispositivo Android 16;
- `clientAndroid10`: cliente executado no dispositivo Android 10;
- servidor: executável Linux;
- comunicação: carga, chave pública e assinatura por TCP;
- matriz: 7 algoritmos × 3 tamanhos × quantidade de repetições; o
  padrão de 30 repetições resulta em 630 execuções.

As dependências externas ficam em `third_party/openssl` e
`third_party/liboqs`.

## Dependências nativas

As versões usadas pelo projeto são OpenSSL 3.5.7 e liboqs 0.16.0, preservadas
como submódulos em `third_party/`. Para compilá-las para Linux, execute:

```bash
./tools/build-openssl.sh linux
./tools/build-liboqs.sh linux
```

Os scripts usam Clang e geram compilações `Release` estáticas. Os artefatos e
os cabeçalhos são instalados, respectivamente, em:

- `build/dependencies/linux/openssl`;
- `build/dependencies/linux/liboqs`.

O OpenSSL é compilado sem aplicativos, testes, documentação, bibliotecas
compartilhadas ou módulos dinâmicos. A liboqs é compilada sem depender do
OpenSSL e inclui somente `SIG_falcon_512` e `SIG_falcon_1024`. Todo o conteúdo
gerado fica sob `build/` e não é versionado.

Os mesmos scripts possuem o alvo `android`, preparado para API mínima 29 e
para as ABIs `arm64-v8a` e `x86_64`. Os artefatos para ambas as ABIs foram
gerados em `build/dependencies/android/<ABI>/`; `arm64-v8a` corresponde aos
dois aparelhos físicos confirmados e `x86_64` atende aos emuladores
auxiliares. O diretório de saída pode ser alterado com
`TCC_DEPENDENCIES_DIR`, e o paralelismo com `TCC_BUILD_JOBS`.

A compilação Linux foi validada com Clang 21.1.8. Um teste de ligação e
execução confirmou a disponibilidade de ML-DSA-44/65/87, RSA-PSS e Ed25519
no `libcrypto.a`, além de Falcon-512/1024 no `liboqs.a`.

## Estruturas criptográficas compartilhadas

O contrato C++ usado pelo servidor e pela camada JNI está em
`native/include/tcc/crypto/`. Ele é composto por:

- `bytes.hpp`: representação binária comum (`Bytes`);
- `algorithm.hpp`: identificadores, nomes, provedores e a ordem fixa dos sete
  algoritmos;
- `key_pair.hpp`: par de chaves pública e privada;
- `error.hpp`: códigos e exceção comum para falhas criptográficas;
- `signature_algorithm.hpp`: interface abstrata para geração de chaves,
  assinatura e verificação, incluindo os tamanhos necessários ao transporte.

O cabeçalho `tcc/crypto.hpp` agrega esses componentes e preserva um único
ponto de inclusão para os consumidores. O provedor é derivado do identificador
do algoritmo: ML-DSA-44/65/87, RSASSA-PSS e Ed25519 são associados ao OpenSSL;
Falcon-512/1024, à liboqs. A constante `algorithm_test_order` materializa no
código a ordem experimental documentada.

O teste `crypto_structures_test` verifica nomes, ordem, associação de
provedores, erro para identificador não suportado e uso polimórfico da
interface.

### Adaptadores concretos

A função `create_signature_algorithm()` cria a implementação correspondente a
qualquer um dos sete identificadores. Os adaptadores usam os seguintes formatos
e parâmetros:

- ML-DSA-44/65/87: API EVP do OpenSSL, chaves pública e privada no formato
  binário bruto definido pelo FIPS 204 e assinaturas de tamanho fixo;
- Ed25519: API EVP do OpenSSL, chaves brutas de 32 bytes e assinatura de 64
  bytes;
- RSASSA-PSS: API EVP do OpenSSL, RSA de 3.072 bits, expoente 65.537, SHA-256,
  MGF1-SHA-256 e sal de 32 bytes; chave pública DER `SubjectPublicKeyInfo`,
  chave privada DER PKCS#8 e assinatura de 384 bytes;
- Falcon-512/1024: API de assinatura da liboqs, chaves no formato binário bruto
  da biblioteca e assinaturas compactadas de tamanho variável.

O CMake exige exatamente OpenSSL 3.5.7 e liboqs 0.16.0 nos prefixos gerados
pelos scripts, liga somente `libcrypto.a` e `liboqs.a` e não consulta uma
instalação OpenSSL do sistema. Isso evita combinar o arquivo estático do projeto
com metadados ou dependências de outra versão.

Em 17 de setembro de 2026, `crypto_adapters_test` executou geração de chaves,
assinatura e verificação para os sete algoritmos. O teste também confirmou a
rejeição de carga e assinatura alteradas, chaves com tamanho inválido e
identificador não suportado. A suíte Linux terminou com 3/3 testes aprovados.
As variantes `clientAndroid16Debug` e `clientAndroid10Debug` foram compiladas e
ligadas com todos os adaptadores para `arm64-v8a` e `x86_64`. A execução das
operações criptográficas dentro do Android permanece como validação posterior;
esta etapa confirmou sua compilação e ligação, não medições ou execução no
aparelho.

### Medição das operações criptográficas

O cabeçalho `tcc/measurement.hpp` oferece funções separadas para medir geração
de chaves, assinatura e verificação. Cada função retorna o resultado da
operação junto à sua duração em nanossegundos. A medição usa
`std::chrono::steady_clock` e abrange somente a chamada ao adaptador
criptográfico; geração da carga, metadados, comunicação e persistência ficam a
cargo do executor e fora da região temporizada. Erros dos adaptadores não são
ocultados e continuam sendo propagados como `CryptoError`.

Antes das amostras, `tcc::initialize_crypto_runtime()` prepara o OpenSSL,
mantém o provider `default` e as implementações criptográficas disponíveis,
inicializa os geradores aleatórios da thread e chama `OQS_init()` para
antecipar a detecção de recursos da CPU. Também prepara o backend SHAKE-256
da liboqs, inicializando e liberando um contexto vazio, sem absorver dados,
finalizar, produzir saída ou consumir aleatoriedade. A API está em
`tcc/crypto/runtime.hpp` e deve ser chamada na thread das medições. Ela pode
ser chamada novamente sem gerar chaves, assinaturas ou amostras de aquecimento.
O servidor conclui essa preparação antes de escutar; o Android a executa ao
abrir a tela, no mesmo executor do experimento, antes de habilitar o início.
O executor C++ do cliente também a chama para atender outros consumidores.

Os cronômetros continuam abrangendo a chamada completa ao adaptador. Criação
de contextos, alocação de buffers e importação/exportação de chaves continuam
nesse intervalo; a preparação antecipa os custos compartilhados e por thread,
sem eliminar todos os efeitos da primeira execução.

O script `tools/build-liboqs.sh` aplica de forma idempotente o patch
`tools/patches/liboqs-prepare-shake256.patch` à revisão fixada do submódulo.
O patch expõe `OQS_TCC_init_shake256()` e mantém as funções SHA3 internas
à dependência. A preparação é executada uma vez por processo; na configuração
com pthreads, chamadas concorrentes são sincronizadas. Reconstrua a liboqs
para Linux e para cada ABI Android antes de compilar a aplicação. O CMake
rejeita instalações sem essa extensão. O submódulo permanece com alterações
locais reproduzíveis pelo patch, sem trocar sua revisão.

`tools/measure-falcon.cpp` mede dez repetições de uma variante por processo,
com carga de 10 KiB, chaves novas e os cronômetros do núcleo. Os resultados
são emitidos após as amostras; todas as assinaturas são verificadas e, ao
final, uma carga adulterada é rejeitada. Exemplo no Linux:

```bash
clang++ -std=c++17 -O0 -g -I native/include tools/measure-falcon.cpp \
  build/linux-debug/native/libtcc_native.a \
  build/dependencies/linux/openssl/lib/libcrypto.a \
  build/dependencies/linux/liboqs/lib/liboqs.a \
  -ldl -pthread -o build/measure-falcon
./build/measure-falcon 512
./build/measure-falcon 1024
```

Em 19 de setembro de 2026, `measurement_test` percorreu os sete algoritmos,
validando resultados e durações independentes para geração de chaves,
assinatura, verificação válida e rejeição de carga alterada. A suíte Linux
terminou com 4/4 testes aprovados, inclusive em uma compilação limpa. As duas
variantes Android também foram compiladas com sucesso para `arm64-v8a` e
`x86_64`.

### Transporte TCP compartilhado

O cabeçalho `tcc/transport.hpp` fixa a porta `62037` e expõe
`send_signed_payload()` e `receive_signed_payload()` para sockets já
conectados. Cada envio contém, sem cabeçalho adicional, carga, chave pública e
assinatura nessa ordem. O envio trata escritas parciais e encerra somente a
direção de escrita com `shutdown(SHUT_WR)`, permitindo que a resposta percorra
a direção oposta da mesma conexão.

O recebimento recompõe leituras parciais, usa o tamanho de carga conhecido e o
tamanho de chave pública informado pelo adaptador, e lê a assinatura até o fim
da direção de escrita. Mensagens truncadas, campos vazios, tamanhos de chave
incorretos, assinaturas acima do limite e erros de socket são rejeitados. Não
foram introduzidos cabeçalhos, IDs, confirmações ou mensagens de controle.

Em 19 de setembro de 2026, `transport_test` validou em TCP IPv4 de loopback o
fluxo criptográfico de ida e volta, a remontagem de dados enviados em fragmentos
de até um byte, o encerramento prematuro, o limite da assinatura e a validação
da chave pública. A compilação Linux limpa terminou com 5/5 testes aprovados, e
as duas variantes Android foram compiladas para `arm64-v8a` e `x86_64`.

### Gravação das medições em CSV

O cabeçalho `tcc/csv.hpp` permite serializar medições em qualquer
`std::ostream` ou gravá-las diretamente com `MeasurementCsvWriter`. Cada novo
arquivo é truncado, recebe uma única linha de cabeçalho e usa exatamente as
colunas:

```text
repetition,client,side,algorithm,payload_bytes,operation,duration_ns,status
```

Os lados são `client` e `server`; as operações são `keygen`, `sign` e `verify`;
e os estados são `success` e `failure`. O gravador valida repetição, cliente,
algoritmo, tamanho de carga, operação e duração antes de persistir a linha, e
aplica escape CSV a vírgulas, aspas e quebras de linha no nome do cliente. A
gravação é uma chamada separada das funções temporizadas.

Em 19 de setembro de 2026, `csv_test` confirmou a serialização em memória, a
persistência em arquivo, o escape textual e a rejeição de registros inválidos.
A compilação Linux limpa terminou com 6/6 testes aprovados, e as variantes
Android foram compiladas para `arm64-v8a` e `x86_64`.

## Requisitos de configuração e execução

### Rede

- A porta escolhida é a TCP `62037`, definida diretamente no código
  (*hardcoded*) do cliente e do servidor.
- A porta pertence à faixa dinâmica/privada e não possui serviço registrado.
  Ela também foi verificada como livre na máquina de desenvolvimento. Como a
  disponibilidade de uma porta fixa não pode ser garantida em toda máquina, o
  servidor deve informar claramente uma eventual falha de `bind`.
- O servidor deve escutar em todas as interfaces de rede por meio de
  `0.0.0.0`/`INADDR_ANY`, aceitando conexões destinadas a qualquer um de seus
  endereços IPv4.

### Ordem dos testes

Cliente e servidor devem executar a mesma sequência predeterminada:

1. ML-DSA-44;
2. ML-DSA-65;
3. ML-DSA-87;
4. Falcon-512;
5. Falcon-1024;
6. RSASSA-PSS;
7. Ed25519.

Os tamanhos de carga são executados em ordem crescente: 10 KiB, 100 KiB e
1 MiB. Para cada tamanho, o sistema percorre os sete algoritmos na ordem acima
e conclui todas as repetições configuradas de um algoritmo antes de avançar
para o próximo. O tamanho da carga somente muda depois que os sete algoritmos
forem concluídos para o tamanho atual.

O gerador C++ produz cada carga preenchendo todos os bytes com o valor fixo
`0x65` e rejeita tamanhos diferentes dos três tamanhos definidos.

### Quantidade de repetições

- Cliente e servidor usam 30 repetições por combinação como valor padrão.
- O servidor permite alterar o valor na inicialização com
  `tcc_server --repetitions <quantidade>`.
- O aplicativo cliente oferece uma opção para alterar o valor antes do início
  do experimento.
- Não haverá negociação nem transmissão dessa configuração entre cliente e
  servidor. O sistema pressupõe que ambos foram configurados com a mesma
  quantidade.
- Com `N` repetições, a execução completa possui `7 × 3 × N = 21N`
  repetições; com o padrão `N = 30`, são 630.

### Servidor Linux

Compile o servidor e os testes com o preset Linux:

```bash
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug
```

O executável é gerado em `build/linux-debug/server/tcc_server`. Ele escuta em
`0.0.0.0:62037`, aceita uma conexão por repetição e percorre primeiro os três
tamanhos de carga e, dentro de cada tamanho, os sete algoritmos na ordem fixa.
O código reutilizável fica em `tcc_server_lib`: `config.cpp` trata a linha de
comando e `server.cpp` contém o socket de escuta e o laço experimental. O
`main.cpp` limita-se a coordenar essas partes e traduzir falhas em códigos de
saída.

As opções disponíveis são:

```bash
./build/linux-debug/server/tcc_server \
  --repetitions 30 \
  --client clientAndroid16 \
  --output server_measurements.csv
```

`--repetitions` usa 30 por padrão. `--client` define o identificador registrado
no CSV e usa `unspecified` quando omitido; `--output` seleciona o arquivo de
medições. Use `--help` para consultar essas opções sem iniciar a escuta. O
valor padrão de cliente serve apenas ao desenvolvimento; uma coleta real deve
informar explicitamente `clientAndroid16` ou `clientAndroid10`.

Para cada conexão, o servidor recebe carga, chave pública e assinatura, mede a
verificação e, se ela for válida, mede a geração de um novo par de chaves e a
assinatura da resposta. A transmissão e a gravação permanecem fora das regiões
temporizadas. Três linhas são gravadas e descarregadas no CSV ao fim de cada
tentativa. Operações não alcançadas por causa de uma falha são registradas com
`status=failure` e duração zero, portanto devem ser excluídas das amostras de
desempenho.

`server_config_test` cobre os padrões, todas as opções e entradas inválidas. A
suíte Linux da etapa do servidor possuía sete testes, e um teste de fumaça
adicional confirmou o `bind` em `0.0.0.0:62037`. A integração foi validada
posteriormente em emulador Android 16 e nos dois aparelhos físicos, conforme
a seção abaixo.

## Cliente Android — primeira implementação funcional

O aplicativo agora oferece endereço IPv4 do servidor, quantidade de repetições
(30 por padrão), início, progresso, cancelamento e exportação do CSV. O fluxo
foi implementado a partir do `clientAndroid16`, mantendo Kotlin/JNI e executor
C++ compartilhados com `clientAndroid10`.

`native/src/client.cpp` executa a matriz de `21N` tentativas na mesma ordem do
servidor, com uma conexão TCP por tentativa. A JNI executa em uma thread de
trabalho; geração de chaves, assinatura e verificação são medidas separadamente
pelas funções existentes. Rede, persistência e interface ficam fora dos tempos
criptográficos. O CSV é gravado incrementalmente no armazenamento privado,
com três linhas por tentativa, e pode ser exportado após conclusão, erro ou
cancelamento.

Para compilar as duas variantes e verificar o Android 16:

```bash
source android-env.sh
cd android
./gradlew :app:assembleClientAndroid16Debug :app:assembleClientAndroid10Debug \
  :app:lintClientAndroid16Debug
```

O APK principal fica em
`android/app/build/outputs/apk/clientAndroid16/debug/app-clientAndroid16-debug.apk`.
Para um piloto, inicie o servidor com `--repetitions 1 --client clientAndroid16`,
informe seu IPv4 no aplicativo (`10.0.2.2` no emulador padrão) e selecione `1`
repetição. O resultado esperado é `21 / 21`, com 63 medições por lado.

O cliente encerra a execução no primeiro erro, pois o transporte sem negociação
não permite ressincronizar uma conexão perdida. Após erro ou cancelamento,
reinicie o servidor antes de outra matriz. Operações não alcançadas ficam com
duração zero e `status=failure`; repetições incompletas devem ser excluídas da
análise. Mantenha o aplicativo em primeiro plano durante a coleta. O executor
sobrevive à recriação da Activity, mas não retoma uma execução após o processo
ser encerrado pelo sistema.

Em 22 de setembro de 2026:

- a suíte Linux concluiu **8/8 testes**, incluindo matriz cliente-servidor com
  `N=2`, validação dos CSVs, resposta adulterada, conexão recusada e cancelamento;
- as duas variantes debug compilaram para `arm64-v8a` e `x86_64`; o lint Android
  16 terminou sem erros;
- o piloto no emulador Android 16 concluiu **21/21 tentativas sem falhas**,
  produzindo **63 medições válidas em cada lado**, com correlação conferida;
- o cancelamento pela interface preservou o CSV parcial e liberou os controles.

Também em 22 de setembro de 2026, após a instalação dos APKs nos aparelhos
físicos, o usuário confirmou que ambos os clientes se conectaram ao servidor
Linux desta máquina, realizaram a comunicação e exportaram seus arquivos CSV:

| Cliente | Aparelho | Conexão e comunicação | Exportação CSV |
| --- | --- | --- | --- |
| `clientAndroid16` | Samsung SM-A556E, Android 16/API 36 | Sucesso confirmado pelo usuário | Sucesso confirmado pelo usuário |
| `clientAndroid10` | Xiaomi Redmi Note 8, Android 10/API 29 | Sucesso confirmado pelo usuário | Sucesso confirmado pelo usuário |

Esse registro conclui a validação funcional de comunicação e exportação nos
dois aparelhos, incluindo a exportação que o seletor simulado do AVD
`aosp_atd` não permitia testar. A quantidade de repetições e o conteúdo dos
CSVs dessas execuções físicas não foram informados neste relato; a conferência
da matriz e a coleta final de desempenho permanecem pendentes. Os tempos do
emulador não são resultados de desempenho.

A revisão da base, os arquivos modificados, as decisões, a reprodução do piloto
e as limitações estão em [docs/android-client.md](docs/android-client.md).

## Configuração Android local

Carregue as variáveis da sessão com:

```bash
source android-env.sh
```

O arquivo `android/local.properties` aponta para o SDK instalado nesta máquina
e não deve ser versionado.

Os APKs de depuração incluem `x86_64` para testes em emuladores. A validação
funcional final e todas as medições de desempenho devem usar os aparelhos
físicos correspondentes.

### Dispositivos físicos confirmados

Informações consultadas por ADB em 17 de setembro de 2026 para o
`clientAndroid16` e em 20 de setembro de 2026 para o `clientAndroid10`:

| Cliente | Fabricante | Modelo | Android/API | ABI principal | ABIs suportadas | Hardware/plataforma | Patch de segurança |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `clientAndroid16` | Samsung | SM-A556E | 16/36 | `arm64-v8a` | `arm64-v8a` (somente 64 bits) | `s5e8845`/`erd8845` | 2026-07-05 |
| `clientAndroid10` | Xiaomi | Redmi Note 8 | 10/29 | `arm64-v8a` | `arm64-v8a`, `armeabi-v7a`, `armeabi` | `qcom`/`trinket` | 2021-02-01 |

Os números de série retornados pelo ADB não são registrados, pois não são
necessários para reproduzir a compilação. Para ambos os aparelhos, as
dependências nativas e o aplicativo devem ser compilados para `arm64-v8a`.
