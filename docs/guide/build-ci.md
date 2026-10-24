# Build, Testing & CI/CD

## Локальная сборка

```bash
python -m pip install -r tools/requirements.txt
cmake --preset debug
cmake --build --preset debug --parallel
ctest --test-dir build/debug --output-on-failure
```

В `CMakePresets.json` есть `debug` и `release`, генератор Ninja. В CMake выставлен C++23 для обнаружения модулей; compile flag `-std=c++2d` передаётся Clang явно. CMake 4.5+ обязателен для текущего файла. Поддержка C++29 компилятором частичная и экспериментальная.

## Опциональные targets

- `ARCADE_UI_ENABLE_UNICODE` — находит ICU; включает `arcade_ui_unicode`, `arcade_ui_text` и другие текстовые зависимости.
- `ARCADE_UI_ENABLE_FONT_BACKEND` — ищет FreeType + HarfBuzz; подключает `arcade_ui_font`.
- `BUILD_TESTING` — включает CTest.

Отсутствие ICU или шрифтовых dev-пакетов не должно ломать базовый runtime.

## GitHub Actions

Workflow `.github/workflows/ci.yml` запускается на push/PR в `master`. Он проверяет Python unit tests, компилирует HTML пример в ArcUXI, публикует workflow artifacts и формирует ZIP исходников и SHA256. Для тегов `v*` предусмотрен GitHub Release.

**Важно:** на текущем этапе workflow **не собирает** все нативные C++ targets и не проверяет сборку Clang 23.1.3/CMake 4.5 или связку ICU/HarfBuzz/FreeType. Успешный CI не следует интерпретировать как подтверждение всей платформы.

Сайт документации собирается отдельно через `.github/workflows/pages.yml` из папки `docs/`, без Node/npm и CSS-библиотек. Публикация требует, чтобы в настройках GitHub Pages источником был выбран **GitHub Actions**. При отсутствии прав workflow может потребоваться включение Pages владельцем репозитория.

## Следующие проверки

Добавить C++ CI matrix, sanitizer builds, fuzz для `.arcuxi`, проверку совместимости ABI/пакетного формата, тесты Unicode shaping и бюджет времени layout/shaping.
