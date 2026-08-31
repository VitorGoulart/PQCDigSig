# Ambiente Linux fixado

Versões registradas para reprodução da configuração atual:

| Ferramenta | Versão |
|---|---|
| C++ | C++17 |
| Clang | 21.1.8 (`Ubuntu clang 21.1.8 (6ubuntu1)`) |
| CMake do host | 4.2.3 |
| Ninja | 1.13.2 |
| Python | 3.14.4 |
| OpenJDK disponível | 25.0.2 |
| OpenSSL | 3.5.7 LTS |
| liboqs | 0.16.0 |

O CMake Android permanece fixado separadamente em 4.1.2. Java/Kotlin produzem
bytecode compatível com Java 17, mesmo que o `java` atualmente encontrado no
`PATH` seja OpenJDK 25.0.2.
