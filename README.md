# ReadyCAD

Параметрическое проектирование корпусной мебели: модель → деталировка → присадка → раскрой →
DXF/PDF → управляющие программы для ЧПУ. Дипломный проект. C++20, CMake, OpenGL 3.3, Dear ImGui.
Интерфейс на турецком языке.

Архитектура описана в [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

![ReadyCAD](docs/images/stage5-door-selected.png)

## Что нужно

* CMake ≥ 3.25 и Ninja
* Компилятор с поддержкой C++20: GCC ≥ 11 (MinGW-w64), Clang ≥ 14 или MSVC 2022
* Интернет при первой конфигурации: glm, Catch2, GLFW, ImGui, nlohmann/json,
  nativefiledialog-extended, libHaru и Clipper2 скачиваются через FetchContent в
  `build/<пресет>/_deps` и потом берутся оттуда. У каждого пресета своя копия: зависимости
  debug- и release-сборки компилируются с разными флагами и не должны смешиваться

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
  --pdf-dialog              сразу открыть настройки PDF
  --nesting <saw|router>    сразу рассчитать раскрой для пильного центра или ЧПУ
  --stock                   показать склад листов
  --cutting-settings        сразу открыть настройки раскроя
  --export-pdf <file.pdf>   записать PDF-документацию (с раскроем, если он рассчитан) и выйти
  --data-dir <folder>       брать склад и настройки из этой папки (демо, проверки)
  --screenshot <file.bmp>   сохранить окно в картинку и выйти
```

Например, раскрой гардероба с ящиками на ЧПУ сразу в PDF:
`ReadyCAD.exe --catalog 1 --nesting router --export-pdf gardirop.pdf`.

Горячие клавиши: Ctrl+Z / Ctrl+Y — отмена и повтор, Ctrl+S — сохранить, Ctrl+O — открыть,
Ctrl+P — документация в PDF, F5 — рассчитать раскрой, Delete — удалить выбранную полку, дверь
или блок ящиков.

Меню «Dışa aktar»: PDF-документация (деталировка, фурнитура, раскрой, сборочный чертёж,
чертежи деталей), все детали в одном DXF или каждая деталь в своём DXF для CAD/CAM, раскладка
каждого листа в своём DXF.

Меню «Kesim» и окно «Kesim planı»: раскрой на пильном центре или нестинг на ЧПУ, прогресс
генетического алгоритма, карта каждого листа, списание листов со склада. Окно «Levha stoku» —
склад листов и деловых остатков.

![Раскрой на пильном центре](docs/images/stage7-saw.png)

Склад и настройки раскроя принадлежат мастерской, а не проекту. Они лежат в папке
пользователя и общие для всех проектов: на Windows `%APPDATA%\ReadyCAD\stock.rcstock` и
`settings.json`, в Linux `~/.config/ReadyCAD`.

Тест `gui_smoke_screenshot` (метка `gui`) запускает настоящее окно. На машине без OpenGL 3.3
его можно пропустить: `ctest --preset debug -LE gui`.
