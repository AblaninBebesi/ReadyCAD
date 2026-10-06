# ReadyCAD

Параметрическое проектирование корпусной мебели: модель → деталировка → присадка → раскрой →
DXF/PDF → управляющие программы для ЧПУ. Дипломный проект. C++20, CMake, OpenGL 3.3, Dear ImGui.
Интерфейс на турецком языке.

Архитектура описана в [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## Что нужно

* CMake ≥ 3.25 и Ninja
* Компилятор с поддержкой C++20: GCC ≥ 11 (MinGW-w64), Clang ≥ 14 или MSVC 2022
* Интернет при первой конфигурации: glm, Catch2, GLFW и ImGui скачиваются через FetchContent
  в `build/_deps` и потом берутся оттуда

## Сборка и тесты

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

Приложение: `build/debug/bin/ReadyCAD.exe`. Тесты: `build/debug/bin/readycad_*_tests.exe`.
Тесты можно запускать и напрямую, например `readycad_core_tests.exe "[aabb]"`.

Пресеты:

| Пресет     | Что собирается |
|------------|----------------|
| `debug`    | приложение и тесты, отладка |
| `release`  | приложение и тесты, оптимизация, без консольного окна |
| `headless` | только ядро и тесты, без GUI; предупреждения считаются ошибками (CI) |

На Windows с MinGW `C:\mingw64\bin` должен быть в `PATH` (там лежат gcc и ninja).
Исполняемые файлы линкуются статически, поэтому DLL MinGW рядом с ними не нужны.
