# OXCE Custom GUI framework 0.5

This branch adds a declarative GUI extension point to **OXCE 8.6.0**. It is
based on upstream commit `3e722e09b655ab67acfb59fc8585dc70b583826c`
(2026-04-26) and keeps the normal 8.6.0 engine/ruleset compatibility numbers
for the XPiratez v.o1 line.

Version 0.5 supplies positioned and styled screens, local values and form
controls, copied read-only game-data tables, controlled scripts, and screen
navigation in Geoscape, Basescape, and Battlescape. A ruleset never receives a
raw C++ widget pointer or unrestricted state constructor.

## Discovery and contexts

A mod declares screens below the top-level `customUis:` list. When at least one
screen exists for a context, the engine exposes a custom-interface entry:

- Geoscape adds a direct **MOD UIS** button beside **EXT**/**FUNDS** and also
  lists them in Extended Links.
- Battlescape lists them in Extended Links; right-clicking the Extended
  Links/layer button opens the custom-interface picker directly.
- Basescape adds **MOD UIS** beside its **GEOSCAPE** link.
- Screens are globally addressable by ruleset ID after normal mod load order
  and override processing. Therefore one mod can open another mod's schema
  screen by ID when that dependency is installed.

`context` is `geoscape`, `basescape`, or `battlescape`. A screen may open
another context's schema screen only when its required runtime context exists:
Battlescape needs an active battle and Basescape needs a current base.

## Screen schema

| Field | Required | Meaning |
| --- | --- | --- |
| `id` | yes | Stable global ruleset ID and override key |
| `title` | no | Localized string ID; defaults to `id` |
| `context` | no | `geoscape`, `basescape`, or `battlescape`; defaults to `geoscape` |
| `interface` | no | Existing OXCE interface theme; defaults to `geoscape` |
| `backgroundImage` | no | Surface ID, including a mod surface supplied with `extraSprites` |
| `width`, `height` | no | Window size: 64..320 by 40..200 |
| `position` | no | Screen anchor plus signed `x`/`y` offsets |
| `autoScrollbars` | no | Automatically place vertical scrollbars inside overflowing tables and word-wrapped labels |
| `values` | no | Screen-instance-local bool, int, and string values |
| `actions` | no | Named close, navigation, or script actions |
| `widgets` | yes | Widgets in drawing order |
| `scripts.customUiAction` | no | Controlled action script for this screen |

`position.anchor` accepts `topLeft`, `top`, `topRight`, `left`, `center`,
`right`, `bottomLeft`, `bottom`, or `bottomRight`. The offsets are applied
after anchoring. The resolved rectangle must remain inside OXCE's 320x200
virtual UI area and scales through OXCE's existing display scaling.

If `backgroundImage` is absent, the selected `interface` supplies the normal
background. Battlescape uses the interface alternative background or
`TAC00.SCR`. A custom background replaces that source.

With `autoScrollbars: true`, table/list scrollbars are kept inside the
widget's right edge and column widths are proportionally fitted when needed to
reserve scrollbar space. A word-wrapped label becomes a non-selectable text
region with an automatically hidden/shown vertical scrollbar and mouse-wheel
scrolling. Horizontal overflow in those labels is handled by wrapping rather
than a horizontal scrollbar. Dropdown popups always render above ordinary
widgets and choose their normal OXCE popup height from the available options.

Normal OXCE `new`, `override`, `update`, `delete`, and `refNode` processing
applies. Invalid types, IDs, bindings, dimensions, colors, sources, columns,
actions, or navigation modes fail during ruleset loading.

## Local values and bindings

Values are recreated each time a screen instance opens:

```yaml
values:
  enabled: { type: bool, default: true }
  amount: { type: int, default: 5 }
  query: { type: string, default: "" }
  sortField: { type: string, default: name }
```

`toggle` requires a bool binding; `number` requires int; `textInput`,
`search`, and `dropdown` require string. `${valueId}` in label, button, or
toggle text is replaced whenever bound values synchronize.

These values are deliberately transient. They do not change the save unless a
purpose-built script API is added later for that operation.

## Widgets

All widgets require unique `id`, positive `width` and `height`, and
window-relative `x` and `y`.

| Type | Main fields |
| --- | --- |
| `label` | `text`, `big`, `align`, `wordWrap`, `overflow`, `autoScrollbar` |
| `button` | `text`, `action` (or `onClick`), `focused`, `selected` |
| `toggle` | `text`, bool `bind`, `onChange`, `focused` |
| `textInput` | string `bind`, `placeholder`, `maxLength`, `onChange`, `onSubmit`, `focused` |
| `search` | Same form behavior as `textInput`; may be linked from a list |
| `number` | int `bind`, `minimum`, `maximum`, `step`, `largeStep`, `wrap`, `mouseWheel`, `onChange`, `onSubmit` |
| `dropdown` | string `bind`, `options`, `onChange` |
| `list` / `table` | `source`, `columns`, search/sort/selection fields, `onSelect` |

Numeric input has mouse up/down arrows. Mouse wheel uses `step`; Shift or Ctrl
with an arrow/wheel uses `largeStep`. Typed values are clamped on submit.

Dropdown options may be localized string IDs or explicit text/value pairs:

```yaml
options:
  - { text: STR_NAME, value: name }
  - { text: STR_QUANTITY, value: quantity }
```

Escape closes the screen. Tab and Shift+Tab move button/toggle focus; Enter or
Space activates the focused button. Text fields retain normal text-entry
behavior.

Label `overflow` defaults to `vertical`: text that is wider than its declared
width wraps onto following lines. `overflow: horizontal` currently uses the
same vertical fallback because OXCE has no reusable horizontal text scrollbar
or horizontally pannable text surface. With screen-level
`autoScrollbars: true`, an explicitly declared overflow label becomes a
scrollable text region when its wrapped height exceeds the viewport.
Alternatively, set `autoScrollbar: true` directly on one label. Its vertical
scrollbar is placed inside the label's right edge, and the wrapped text width
is reduced so neither the text nor scrollbar overlaps adjacent widgets.

## Tables and controlled data providers

Tables receive copied records on refresh; they do not retain savegame object
pointers. A provider can be a scalar:

```yaml
source: soldiers
```

or a map:

```yaml
source:
  type: items
  scope: currentBase
  includeZero: false
```

`scope` is `currentBase` or `all`. It applies to base-owned providers
(`items`, `soldiers`, and `crafts`). `includeZero` applies to items.

| Provider | Stable column/sort fields |
| --- | --- |
| `research` | `id`, `name`, `cost`, `points`, `discovered`, `discoveredValue`, `statusId`, `status` |
| `items` | `id`, `name`, `quantity`, `buyCost`, `sellCost`, `size` |
| `soldiers` | `id`, `name`, `rank`, `base`, `craft`, `missions`, `kills`, `woundRecovery` |
| `bases` | `id`, `name`, `soldiers`, `crafts`, `scientists`, `engineers`, `usedStores`, `availableStores` |
| `crafts` | `id`, `name`, `type`, `status`, `base`, `fuel`, `damage`, `soldiers` |

Example:

```yaml
- id: inventory
  type: table
  x: 8
  y: 60
  width: 288
  height: 82
  source: { type: items, scope: currentBase }
  search: itemSearch
  selectedValue: selectedItem
  sortValue: sortField
  descendingValue: descending
  columns:
    - { field: name, header: STR_NAME, width: 188 }
    - { field: quantity, header: STR_QUANTITY, width: 48, align: right }
    - { field: sellCost, header: STR_SELL_VALUE, width: 48, align: right }
  onSelect: inspectSelection
```

`search` references a `search` or `textInput` widget. Filtering checks the
record ID and every exposed string field, case-insensitively. `sortBy` is a
fixed field; `sortValue` references a string value for user-controlled sort.
`descendingValue` references bool. `selectedValue` receives the stable record
ID. Lists accept one through eight columns whose widths do not exceed the
widget width.

A column with `header` renders a native OXCE heading inside the table's top
row. When the table also declares `sortValue`, the engine adds the same
`ARROW_SMALL_UP` / `ARROW_SMALL_DOWN` button used by Stores. Clicking a new
heading selects that field ascending; clicking the active heading toggles
descending. `descendingValue` mirrors the current direction when supplied.
The header row is included in the table widget's declared height.

## Declarative actions and navigation

Named actions support:

```yaml
actions:
  closeThis: { type: close }
  more:
    type: openCustomUi
    target: STR_OTHER_CUSTOM_UI
    mode: push
  stores:
    type: openNativeUi
    target: stores
    mode: push
  inspectSelection: { type: script }
```

`push` keeps the current screen beneath the next one; closing the child returns
to it. `replace` removes the current screen first. The literal widget action
`close` remains a shorthand.

This provides the requested routes:

- schema UI -> same mod's schema UI: `openCustomUi`
- schema UI -> another mod's schema UI: `openCustomUi` with its global ID
- schema UI -> supported existing game UI: `openNativeUi`
- schema UI -> engine/fork-added UI: register a native factory, then use
  `openNativeUi`

Built-in native IDs are `ufopaedia`, `techTree`, `research`, `manufacture`,
`soldiers`, `crafts`, and `stores`. Base screens require a current base.

Arbitrary C++ class names are intentionally not constructible from YAML.
Engine code can expose another screen through
`CustomUiState::registerNativeScreen(id, factory)`. This preserves a narrow,
reviewable boundary and also supports screens added by another compiled fork.

Targets are resolved when the action runs. A missing optional cross-mod target
is logged and the current screen stays open.

## Action scripts

A `script` action invokes the screen's `customUiAction` hook after its widget
has updated its local binding. The hook inputs are:

| Input | Meaning |
| --- | --- |
| `ui` | Writable controlled interface for the current screen |
| `screen_id` | Current custom UI ID |
| `action_id` | Named action that invoked the hook |
| `widget_id` | Widget that invoked it |
| `selected_record_id` | Last selected table record ID, or empty |
| `geoscape_game` | SavedGame pointer when available |
| `battle_game` | SavedBattleGame pointer in Battlescape, otherwise null |
| `rules` | Read-only Mod/rules pointer |

Available operations are:

| Script operation | Purpose |
| --- | --- |
| `ui.getInt ui result valueId` | Read int/bool into `result` |
| `ui.getText ui valueId result` | Read string into `result` |
| `ui.setInt ui valueId value` | Set int/bool and synchronize widgets |
| `ui.setText ui valueId value` | Set string and synchronize widgets |
| `ui.setWidgetText ui widgetId text` | Override a label/button/toggle caption |
| `ui.refresh ui widgetId` | Refresh one table; empty ID refreshes all |
| `ui.close ui` | Close after the script returns |
| `ui.open ui screenId replace` | Open a schema screen after return |
| `ui.openNative ui screenId replace` | Open a registered native screen after return |

OXCE script functions use receiver-first Polish notation. A minimal local hook:

```yaml
scripts:
  customUiAction: |
    ui.setWidgetText ui "status" "STR_CUSTOM_UI_EXAMPLE_SCRIPT_RAN";
    return;
```

Navigation is deferred until the script returns so the current state is never
deleted while its action is executing. If a script queues more than one
navigation operation, the last operation wins.

## Colors and formatted text

Widgets may declare interaction palettes:

```yaml
colors:
  default: { background: 96, text: "#ffffff" }
  hover: { background: 112, text: "#fff2a8" }
  focused: { background: 80 }
  selected: { background: 64 }
```

Colors are palette indices 1..255 or quoted `#RRGGBB`. Since OXCE renders an
8-bit palette, RGB is mapped to the nearest active palette entry. Missing
state values inherit from `default`, then from the interface. State priority
is hover, focused, selected, default.

Localized label/button/toggle strings support nested scoped tags:

```text
Normal [color=#70d6ff]cyan emphasis[/color] normal again.
```

Invalid tags and unmatched closing tags remain literal. An unclosed opening
tag colors the rest of the string.

## Current limits

- Widgets draw in declaration order. There are no containers, explicit
  `zIndex`, clipping policies, or pointer pass-through controls yet.
- Window anchoring is implemented; widget anchors, percentage size, and
  responsive reflow are not.
- Screen background images work; standalone image widgets are not included.
- Default, hover, focus, and selected visual states work. Disabled/hidden and
  validation-error states are not yet data-driven.
- Values are local to one screen instance; pushing another screen does not
  implicitly share them.
- The research provider is generic table data only. The requested research
  screen/graph overhaul is intentionally deferred.

## Compatibility and milestone status

- Base engine: OXCE 8.6.0 / v2026-04-26.
- Intended total-conversion target: XPiratez v.o1.x.
- Existing rulesets and saves require no changes; `customUis:` is optional.
- 0.1 registration/discovery: complete.
- 0.2 layout, styling, backgrounds, states, and all three contexts: complete.
- 0.3 values and form controls: complete.
- 0.4 controlled table providers: complete.
- 0.5 actions, controlled script API, and navigation: complete.
- Specialized research screen/graph work: skipped for this time-sensitive
  branch as requested.
- 1.0 hardening remains: schema version negotiation, focused compatibility
  tests, migration notes, and upstream review.

The framework remains a generic mod-utility UI. Cheat menus are one use case,
alongside accessibility tools, configuration panels, character viewers, and
debugging utilities.
