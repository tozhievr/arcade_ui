# HTML & CSS: поддерживаемый профиль

Arcade UI использует **настоящий HTML/CSS синтаксис**, но только документированный профиль. Это не WebView: нет JavaScript, runtime DOM, Web API, навигации, legacy quirks. Ориентиры: [HTML Living Standard](https://html.spec.whatwg.org/multipage/) и [CSS спецификации](https://www.w3.org/TR/css/).

## HTML

Поддерживаемые элементы в текущем профиле: `div`, `span`, `p`, `b`, `strong`, `i`, `em`, `small`, `label`, `button`, `section`, `header`, `footer`, `main`, `nav`, `article`, `aside`, `ul`, `ol`, `li`, `h1`–`h6`, `a`, `img`, `input`, `br`, `hr`, `form`, `fieldset`, `legend`, `output`, `progress`, `details`, `summary`, `figure`, `figcaption`. `html`, `head`, `body`, `style`, `title` и базовая metadata обрабатываются специально.

`<!doctype html>` и HTML character references поддерживаются входным парсером. Полный WHATWG tree builder пока отсутствует: например, автоматическое исправление ошибочно вложенных таблиц не гарантируется.

```html
<button data-action="save">Save</button>
```

`data-action` задаёт ключ нативного callback, а не JavaScript. `<script>` и HTML `on*=` запрещены.

## CSS

В компиляторе распознаются `display`, `flex-direction`, `width`, `height`, `flex-grow`, `flex-shrink`, `gap`, `padding`, `margin`, `font-size`, `background`, `color`, `justify-content`, `align-items`, `flex-wrap`, `grid-template-columns`, `flex-basis`, `min-width`, `max-width`, `grid-column`.

Селекторы разбираются `cssselect2`, декларации — `tinycss2`. Учитываются specificity, `!important` и inline style. Реализован **не весь** CSS cascade.

```css
.dashboard {
  display: grid;
  grid-template-columns: 120px 1fr 2fr;
  gap: 12px;
}
.card { display: flex; flex-direction: column; padding: 16px; }
.card > button { color: white; background: #36415c; }
```

Grid поддерживает фиксированные `px`, `fr`, `repeat(N, …)`, ограниченный `minmax(px, 1fr)`, `grid-column: span N`. Flex поддерживает grow/shrink с итеративным применением min/max и начальное wrapping/align. Это **не полная нормативная реализация** Flex/Grid.

## Чего нет

CSS custom properties/`var()`, полноценные container/media queries, cascade layers, псевдоэлементы, все виды intrinsic sizing, complete percentages, grid named lines, полноценная CSS Text типографика. Неизвестные свойства/значения в выбранном строгом профиле обычно диагностируются при AOT-компиляции.

[Подробный стандартный профиль](../standards-profile.md).
