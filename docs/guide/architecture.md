# Архитектура

## Поток данных

```text
HTML / CSS / Assets
   │
   ▼
arcuxc.py  ─── errors + source-context diagnostics
   │
   ▼
.arcuxi v3 ─── packed node/style/track/string/resource data
   │
   ▼
arcade.ui::Document
   ├── flat topology + PMR storage
   ├── Flex / Grid / Block subset
   ├── dirty propagation and incremental geometry
   ├── events by identifiers
   └── draw commands / SVG preview
```

Главная оптимизация — парсинг и часть вычислений происходят на этапе компиляции; при запуске интерфейс не нуждается в браузерном DOM. **Плоское хранение не отменяет иерархическую семантику:** у элементов сохраняются родительские связи и диапазоны потомков, необходимые для layout.

## Слои

**Compiler:** Python `tools/arcuxc.py`, `tinycss2`, `cssselect2`, диагностический модуль. При неподдерживаемой конструкции выдаёт ошибку. Неполный HTML tree builder и каскад не следует путать с полным WHATWG/CSS conformance.

**Binary:** детерминированная little-endian упаковка `.arcuxi`. Версия структуры сейчас 3. Доверенные результаты своего компилятора — основной сценарий; для недоверенных файлов требуется дополнительный аудит валидации.

**Runtime:** модуль `arcade.ui` предоставляет `Document` и layout. Геометрия кэшируется; изменения размеров инвалидируют зависимые области, изменения paint не должны требовать полной layout-перекомпоновки.

**Text:** ICU модуль, CSS Text proof-of-concept, bridge к Document, script runs, опциональный OpenType backend, самостоятельный shaped glyph cache. Не все компоненты объединены в один production pipeline.

**Output:** draw commands и SVG. GPU renderer / glyph atlas не реализованы.

## Компромиссы

- Packed layout улучшает локальность, но динамические структурные изменения сложнее.
- Flex/Grid требуют вычислять intrinsic contributions; текущая реализация покрывает только subset.
- Unicode breaks и bidi не равны готовой типографике без shaping, font fallback и baseline.
- Coroutine event scheduler и GPU renderer остаются направлениями разработки, а не готовыми механизмами.
