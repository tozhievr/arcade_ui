# Начало работы

Arcade UI — нативный экспериментальный runtime C++ Modules и AOT-компилятор `tools/arcuxc.py`. Он переводит поддерживаемый HTML/CSS в `.arcuxi`, затем `Document` загружает данные и строит геометрию/команды отрисовки.

## Требования

- CMake **4.5+**, Ninja, Clang с C++ Modules и `-std=c++2d`. Целевой toolchain: Clang 23.1.3 / CMake 4.5.0-rc1; эта конкретная пара не прошла end-to-end подтверждение.
- Python **3.11+**, `tinycss2>=1.5,<2`, `cssselect2>=0.9,<1`.
- Опционально: ICU `uc`/`i18n`, FreeType + HarfBuzz development packages.

```bash
git clone https://github.com/tozhievr/arcade_ui.git
cd arcade_ui
python -m pip install -r tools/requirements.txt
cmake --preset debug
cmake --build --preset debug --parallel
ctest --test-dir build/debug --output-on-failure
```

Для Release: `cmake --preset release` и `cmake --build --preset release --parallel`. Имя программы на Windows — `arcadeui.exe`.

## Минимальный UI

```html
<!doctype html>
<html lang="en">
<head>
  <style>
    main { display: flex; gap: 12px; padding: 24px; }
    button { background: #273047; color: white; }
  </style>
</head>
<body>
  <main>
    <button data-action="start">Start</button>
    <button data-action="settings">Settings</button>
  </main>
</body>
</html>
```

Сохраните как `demo.html`, затем:

```bash
python tools/arcuxc.py demo.html -o demo.arcuxi
./build/debug/arcadeui demo.arcuxi
./build/debug/arcadeui demo.arcuxi preview.svg
```

`arcadeui` — консольная проверка загрузчика/layout с экспортом SVG, **не интерактивное GPU-приложение**. Компилятор `arcuxc.py` запускается при сборке примера через CMake.

## Тесты

```bash
python -m unittest discover -s tests -p 'test_*.py' -v
ctest --test-dir build/debug --output-on-failure
```

Если отсутствуют ICU или HarfBuzz/FreeType, зависимые цели пропускаются. Флаг `-std=c++2d` включает экспериментальный синтаксис компилятора, а не гарантирует полную реализацию C++29.
