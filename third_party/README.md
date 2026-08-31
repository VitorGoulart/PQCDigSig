# Dependências nativas

As fontes da liboqs são mantidas no submódulo `third_party/liboqs`, fixado na
versão 0.16.0. Os artefatos gerados não são versionados.

Para compilar e instalar localmente a variante Linux:

```bash
tools/build-liboqs.sh linux
```

Para Android `arm64-v8a`:

```bash
tools/build-android-dependencies.sh arm64-v8a
```

Os destinos da liboqs são, respectivamente:

```text
third_party/install/liboqs-linux
third_party/install/liboqs-android/arm64-v8a
```

As duas variantes são estáticas, usam `Release`, não vinculam OpenSSL
internamente e contêm somente Falcon-512/1024. ML-DSA-44/65/87 é provido
separadamente pelo OpenSSL. Mantenha essas opções iguais nos dois dispositivos
durante o experimento.

## OpenSSL

O OpenSSL está fixado na versão 3.5.7 LTS e é compilado estaticamente, somente
com os componentes de desenvolvimento necessários. Para Linux:

```bash
tools/build-openssl.sh linux
```

Para Android `arm64-v8a`, o agregador compila tanto a liboqs quanto o OpenSSL:

```bash
tools/build-android-dependencies.sh arm64-v8a
```

Os artefatos locais são instalados em `third_party/install/openssl-linux` e
`third_party/install/openssl-android/arm64-v8a`. O núcleo vincula somente
`libcrypto`; `libssl` não é necessário porque o transporte da aplicação usa TCP
sem TLS. A API EVP de `libcrypto` será usada para ML-DSA-44/65/87,
RSASSA-PSS e Ed25519.
