# Ambiente Android fixado

| Componente | Versão/configuração |
|---|---|
| `compileSdk` / `targetSdk` | 37 |
| `minSdk` | 26 |
| ABI | `arm64-v8a` |
| NDK | 30.0.15729638 |
| Clang do NDK | 21.0.0 |
| CMake Android | 4.1.2 |
| Java/Kotlin target | 17 |
| JDK atualmente no `PATH` | OpenJDK 25.0.2 |
| Android Gradle Plugin planejado | 9.2.0 |
| Gradle Wrapper planejado | 9.4.1 |
| Kotlin | suporte embutido do AGP 9.2.0 |

AGP 9.2.0 e Gradle 9.4.1 foram escolhidos por suportarem oficialmente API 37.
A criação efetiva do Wrapper e a migração do arquivo Gradle para Kotlin
embutido pertencem à Fase 3; esta seção congela as versões que deverão ser
usadas nessa implementação.
