# Arcade UI

**Нативный AOT-движок пользовательских интерфейсов на C++ Modules.** Стандартная разметка HTML и CSS компилируется в компактный бинарный формат `.arcuxi`, который загружается C++ runtime без браузера, JavaScript и runtime DOM.

[![Arcade UI CI](https://github.com/tozhievr/arcade_ui/actions/workflows/ci.yml/badge.svg)](https://github.com/tozhievr/arcade_ui/actions/workflows/ci.yml)
![C++](https://img.shields.io/badge/C%2B%2B-modules-00599C)
![Clang](https://img.shields.io/badge/compiler-Clang-orange)
![Status](https://img.shields.io/badge/status-experimental-yellow)

> **Статус: экспериментальный прототип v1.9.** Проект уже содержит работающие компоненты AOT-компилятора, бинарного загрузчика, Flex/Grid layout и Unicode Text. Он **не** является браузером, полноценным CSS-движком или готовой UI-библиотекой для production.

## Зачем Arcade UI

Вместо HTML/CSS-парсинга при каждом запуске интерфейс проходит предварительную компиляцию. Рантайм получает упакованные узлы, стили и ресурсы, строит геометрию и генерирует команды отрисовки.

```text
HTML + CSS + assets
        │
        ▼
   arcuxc.py (AOT)
   ├── parsing / selectors / diagnostics
   ├── style & layout metadata
   └── resource packaging
        │
        ▼
      .arcuxi
        │
        ▼
 arcade.ui (C++ runtime)
   ├── flat node storage / PMR
   ├── incremental Flex & Grid layout
   ├── events / native callback dispatch
   └── draw commands / SVG preview
        │
        └── optional text modules
            ├── ICU: Unicode analysis
            ├── CSS Text: line fitting & bidi
            ├── FreeType + HarfBuzz: OpenType backend
            └── ShapedRunCache
```

**Главные принципы:** реальный HTML/CSS-синтаксис в поддерживаемом профиле, минимальная работа при запуске, плоские данные вместо объектного DOM, инкрементальные обновления и публичный API на C++ Modules. Полного соответствия HTML Living Standard / CSS Snapshot 2026 пока **нет**.

## Быстрый старт

### Требования

- **CMake 4.5+**, **Ninja**, **Clang** с поддержкой named C++ Modules и опции `-std=c++2d`. Целевая конфигурация проекта — Clang 23.1.3 + CMake 4.5.0-rc1; она ещё не была подтверждена end-to-end тестированием в исходном окружении разработки.
- **Python 3.11+** для AOT-компилятора; зависимости находятся в `tools/requirements.txt`.
- Необязательно: **ICU** (`uc` и `i18n`) для Unicode-модулей; **FreeType** и **HarfBuzz** для OpenType backend. Их development-пакеты и пути поиска должны быть доступны CMake.

```bash
git clone https://github.com/tozhievr/arcade_ui.git
cd arcade_ui

python -m pip install -r tools/requirements.txt

cmake --preset debug
cmake --build --preset debug --parallel
ctest --test-dir build/debug --output-on-failure
```

Если зависимости ICU или HarfBuzz/FreeType не найдены, соответствующие опциональные targets не собираются. Базовые AOT-компилятор и `arcade_ui` остаются доступными.

Сборка с `--preset release` использует каталог `build/release`. CMake устанавливает базовый уровень C++23 для распознавания модулей, а Clang получает `-std=c++2d` явно. Это **не** означает реализацию всего будущего стандарта C++29.

### Скомпилировать HTML/CSS в ArcUXI

```bash
python tools/arcuxc.py examples/dashboard.html -o dashboard.arcuxi
```

Для получения сообщений об ошибках в JSON:

```bash
python tools/arcuxc.py examples/diagnostics_demo.html -o diagnostics.arcuxi --diagnostic-format=json --max-errors=100
```

Диагностика включает коды, уровни серьёзности, местоположение, исходный контекст и подсказки. Source mapping улучшен, но ещё не гарантирует lossless-отображение для всех крайних случаев WHATWG/Unicode.

### Запустить runtime

```bash
./build/debug/arcadeui ./build/debug/dashboard.arcuxi
./build/debug/arcadeui ./build/debug/dashboard.arcuxi --geometry
./build/debug/arcadeui ./build/debug/dashboard.arcuxi preview.svg
```

На Windows исполняемый файл называется `arcadeui.exe`. Это **CLI-демонстрация**, которая загружает интерфейс, рассчитывает layout и умеет экспортировать SVG; полноценного интерактивного GPU-окна здесь пока нет.

## Пример интерфейса

```html
<!doctype html>
<html lang="en">
<head>
  <style>
    .dashboard {
      display: grid;
      grid-template-columns: repeat(3, 1fr);
      gap: 12px;
      padding: 16px;
    }
    button { background: #344054; color: white; }
  </style>
</head>
<body>
  <main class="dashboard">
    <button data-action="start">Start</button>
    <button data-action="settings">Settings</button>
    <button data-action="exit">Exit</button>
  </main>
</body>
</html>
```

`data-action` задаёт идентификатор нативного обработчика, **не** JavaScript. Элемент `<script>` и inline `on*=` обработчики запрещены намеренно.

## Реализовано и ограничения

| Подсистема | Что есть сейчас | Что ещё не реализовано |
| --- | --- | --- |
| HTML/CSS AOT | Разбор поддерживаемого HTML, CSS через `tinycss2` / `cssselect2`, каскад author declarations, строгая диагностика | Полный HTML tree builder, все CSS-модули, media/container queries, custom properties |
| `.arcuxi` | Бинарная упаковка узлов, стилей, Grid tracks, строк и ресурсов (формат v3) | Полная переносимая ABI-спецификация и исчерпывающая валидация недоверенных файлов |
| Layout | Плоское хранение; Block/Flex/Grid subset; `fr`, ограниченный `minmax()`, spanning; dirty propagation | Полные нормативные алгоритмы Flexbox/Grid, все intrinsic contributions и sizing cases |
| Текст | Подключаемые измерители, width-aware строки, начальные `min-content` / `max-content` | Полная CSS Text-типографика, baseline и shaping всех случаев |
| Unicode (ICU) | UAX #14 break opportunities, UAX #9 bidi, UAX #29 grapheme boundaries, script runs | Полное интегрированное форматирование CSS Text |
| OpenType (опционально) | Код backend на FreeType/HarfBuzz с изоляцией заголовков | Полностью проверенная сборка/интеграция, font fallback и реальный GPU glyph renderer |
| Glyph cache | `ShapedRunCache`: PMR SoA, LRU, generations, инвалидация | Интеграция кэша с HarfBuzz и полноценным text renderer |
| Renderer | Draw commands и SVG preview | GPU backend, atlas, batching, accessibility / IME |

**Важно:** существование отдельного Unicode/Text модуля не означает, что он уже автоматически используется во всех случаях обычного `Document`. Например, `DocumentAdapter` подключается явно, а CSS Text style пока не передаётся из `.arcuxi` автоматически.

## Архитектура репозитория

```text
.
├── .github/workflows/ci.yml       # GitHub Actions
├── CMakeLists.txt                  # C++ Modules, native targets, CTest
├── CMakePresets.json               # debug / release, Ninja
├── docs/standards-profile.md      # Контракт стандартов и текущий профиль
├── examples/                      # HTML, .arcuxi и SVG-примеры
├── src/
│   ├── arcade.ui.cppm              # Базовый runtime / layout / Document
│   ├── arcade.ui.font.cppm         # Опциональный OpenType backend
│   ├── arcade.ui.unicode.cppm      # ICU Unicode
│   ├── arcade.ui.text.cppm         # CSS Text fitting
│   ├── arcade.ui.text.document.cppm# Адаптер к Document
│   ├── arcade.ui.text.runs.cppm    # Script & bidi runs
│   ├── arcade.ui.text.cache.cppm   # Shaped glyph cache
│   └── main.cpp                    # CLI / SVG preview
├── tests/                          # Python и C++ регрессии
└── tools/
    ├── arcuxc.py                   # AOT компилятор
    ├── diagnostics.py              # Контекстная диагностика
    └── requirements.txt
```

**C++-модули не требуют включения заголовков ICU/HarfBuzz/FreeType в публичные интерфейсы.** Зависимости изолируются в реализации соответствующих backend-модулей.

## Тестирование и CI/CD

Быстрый запуск Python-тестов:

```bash
python -m unittest discover -s tests -p 'test_*.py' -v
```

Нативные проверки после CMake-сборки:

```bash
ctest --test-dir build/debug --output-on-failure
```

GitHub Actions workflow: [Arcade UI CI](https://github.com/tozhievr/arcade_ui/actions/workflows/ci.yml). Он запускает Python-тесты, компилирует пример `.arcuxi`, сохраняет артефакты и упаковывает исходники. По тегам `v*` настроен шаг публикации GitHub Release.

**Граница CI:** текущий GitHub Actions workflow **не** компилирует C++ targets, не проверяет HarfBuzz/FreeType и не подтверждает работу Clang 23.1.3. Эти проверки остаются задачами развития CI.

## Стандарты и принципы совместимости

Arcade UI ориентируется на:

- [HTML Living Standard](https://html.spec.whatwg.org/multipage/) — стандартный HTML-синтаксис, но сознательно ограниченный набор элементов и поведения;
- [CSS Snapshot / CSS specifications](https://www.w3.org/TR/css/) — стандартные CSS declarations и алгоритмы в пределах реализованного профиля;
- [Unicode UAX #9](https://unicode.org/reports/tr9/), [UAX #14](https://unicode.org/reports/tr14/) и [UAX #29](https://unicode.org/reports/tr29/) — обработка направления и разбиения текста.

Arcade UI **не поддерживает** браузерные legacy-режимы, JavaScript runtime, DOM scripting, navigation и Web API. Неподдерживаемые конструкции должны приводить к диагностике на этапе AOT, а не к тихой имитации браузерного поведения.

Более подробный контракт: [docs/standards-profile.md](docs/standards-profile.md). Некоторые исторические разделы этого документа отражают более ранние версии; актуальную функциональность нужно сверять с исходниками и тестами.

## Дальнейшее развитие

1. Завершить спецификационно точные Flexbox/Grid: intrinsic sizing, auto placement, percentage sizing, baseline.
2. Интегрировать CSS Text, script runs, HarfBuzz shaping и ShapedRunCache в единый pipeline.
3. Добавить font fallback, glyph atlas и нативный GPU renderer.
4. Расширить проверку `.arcuxi` и source maps, включая fuzz/property tests.
5. Запустить C++ CI matrix для целевых Clang, CMake, Ninja и платформ; измерять latency, allocation count, bytes touched и cache hit rate.

---

**Arcade UI развивается как нативная система интерфейсов, а не как встраиваемый браузер.** Производительность и совместимость с HTML/CSS следует оценивать по воспроизводимым тестам, а не по заявленным возможностям архитектуры.
