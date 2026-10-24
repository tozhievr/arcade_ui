# Текст и Unicode

Текстовый pipeline состоит из независимо проверяемых компонентов; **полной интегрированной CSS-типографики пока нет**.

## ICU

Модуль `arcade.ui.unicode` предоставляет:
- UAX #14 line break opportunities;
- UAX #9 paragraph bidi и `resolve_visual_lines()` для bidi каждой готовой строки;
- UAX #29 `grapheme_boundaries()` для безопасного аварийного переноса;
- UTF-8 byte offsets и проверку некорректного UTF-8.

Используются ICU `uc`/`i18n`, но их типы не экспортируются из публичного интерфейса.

## CSS Text

`arcade.ui.text` предоставляет `fit(text, available_width, Style, measure, context)`. Поддержаны начальные режимы `white-space` (`normal`, `nowrap`, `pre`, частично `pre-wrap`, `pre-line`, `break-spaces`) и `overflow-wrap`. Это proof-of-concept: остаются пробелы по границам строк, soft hyphen, полная CSS сегментация, line-height и baseline.

`arcade.ui.text.document` адаптирует этот fitter к `Document`. Стиль нужно передавать вручную: автоматическая связь всех CSS text properties с `.arcuxi` пока не реализована.

## Run segmentation и shaping

`arcade.ui.text.runs` сегментирует сформированные строки по bidi direction и Unicode script; обработка Common/Inherited символов пока эвристическая.

`arcade.ui.font` содержит опциональный FreeType/HarfBuzz backend. Наличие исходников ещё не означает, что проверен полный pipeline на всех скриптах или что font fallback готов.

`arcade.ui.text.cache` хранит shaped glyph IDs, UTF-8 cluster offsets, advances и offsets в SoA с PMR. Записи ограничены по числу и глифам, вытесняются LRU; инвалидируются через generation/face ID. Кэш не thread-safe, возвращённые spans нельзя удерживать после модификации; cache-to-HarfBuzz интеграции пока нет.

## Для production ещё нужны

Правильное shaping целых runs, font fallback по кластерам, line fitting с учётом contextual shaping, caret/hit-testing, line baseline, inline formatting context, GPU glyph atlas и regression suite по реальным шрифтам.
