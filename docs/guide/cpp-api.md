# C++ API

Главный публичный модуль — `arcade.ui`, дополнительная подсистема текста разбита на модули, собираемые отдельными CMake targets.

## Document

```cpp
import arcade.ui;

auto doc = arcade::ui::Document::load("dashboard.arcuxi");
doc.resize(1280, 720);

auto commands = doc.commands();
auto geometry = doc.geometry();
auto stats = doc.layout_stats();
```

`Document` хранит упакованные узлы и состояние layout. Методы из текущего runtime:

```cpp
doc.set_size(4, 220.0f, -1.0f);
doc.resize(1280, 720);
doc.set_background(4, 0xFF123456);
doc.invalidate(4);
```

После изменения layout размеры обновляются через `resize()`; изменение цвета предназначено для paint-only invalidation. Индексы зависят от скомпилированного документа — не храните их как долгосрочные стабильные идентификаторы между AOT-пересборками.

## Измерение текста

```cpp
doc.set_text_measurer(intrinsic_callback, &font_context);
doc.set_line_measurer(line_callback, &line_context);
auto intrinsic = doc.intrinsic_text(4);
auto wrapped = doc.layout_text(4, 160.0f);
```

Callbacks и contexts независимы. Указатели на context не владеют объектами; нельзя разрушать context раньше прикреплённого к `Document` callback.

## Text modules

- `import arcade.ui.unicode;` — Unicode UAX #9/#14/#29 (при наличии ICU).
- `import arcade.ui.text;` — `text::fit()` с callback измерения.
- `import arcade.ui.text.document;` — `DocumentAdapter`, подключаемый явно.
- `import arcade.ui.text.runs;` — script/bidi segmentation.
- `import arcade.ui.text.cache;` — `ShapedRunCache`, SoA/PMR и LRU.
- `import arcade.ui.font;` — опциональный HarfBuzz/FreeType backend.

```cpp
import arcade.ui.text.cache;
using namespace arcade::ui::text;

ShapedRunCache cache{2048, 262144};
ShapeKey key{
    .face_id = 1,
    .face_generation = 1,
    .pixel_size_64 = 1024
};
auto view = cache.get_or_shape("Hello UI", key, shape_callback, &context);
```

`ShapedRunCache` **не thread-safe**, а возвращённые spans могут инвалидироваться при модификации. Кэш не автоматически интегрирован с HarfBuzz. Для точных сигнатур и владения используйте исходники модулей.
