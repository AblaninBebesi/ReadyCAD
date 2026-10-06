# ReadyCAD

Параметрическое проектирование корпусной мебели: модель → деталировка → присадка → раскрой →
DXF/PDF → управляющие программы для ЧПУ. Дипломный проект. C++20, CMake, OpenGL 3.3, Dear ImGui.
Интерфейс на турецком языке.

Архитектура описана в [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

![ReadyCAD](docs/images/stage5-door-selected.png)

## Что нужно

* CMake ≥ 3.25 и Ninja
* Компилятор с поддержкой C++20: GCC ≥ 11 (MinGW-w64), Clang ≥ 14 или MSVC 2022
* Интернет при первой конфигурации: glm, Catch2, GLFW, ImGui, nlohmann/json и
  nativefiledialog-extended скачиваются через FetchContent
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

## Запуск

```
ReadyCAD.exe [параметры]
  --open <file.rcproj>      открыть проект
  --catalog <0..3>          начать с модели каталога
  --holes                   показать отверстия в 3D
  --no-fronts               скрыть фасады
  --select <Kind>           выделить первую деталь вида (Shelf, Door, ...)
  --screenshot <file.bmp>   сохранить окно в картинку и выйти
```

Горячие клавиши: Ctrl+Z / Ctrl+Y — отмена и повтор, Ctrl+S — сохранить, Ctrl+O — открыть,
Delete — удалить выбранную полку, дверь или блок ящиков.

Тест `gui_smoke_screenshot` (метка `gui`) запускает настоящее окно. На машине без OpenGL 3.3
его можно пропустить: `ctest --preset debug -LE gui`.
