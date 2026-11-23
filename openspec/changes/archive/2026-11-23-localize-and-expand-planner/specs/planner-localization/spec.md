## ADDED Requirements

### Requirement: Choose the planner language

The planner SHALL display interface text, pattern and palette names, saved-state messages, accessibility descriptions and cutting exports in English, German or Polish. It SHALL remember the selected language across launches and use the first supported preferred language for a new device.

#### Scenario: Switch a running planner to Polish

- **WHEN** the quilter chooses Polski from the language menu
- **THEN** the app shows Polish text immediately, including Polish diacritics
- **AND** the same quilt stays open and Polish is used on the next launch

#### Scenario: Open a browser with German preferences

- **WHEN** no language is saved and the browser prefers German
- **THEN** the startup page and the planner use German
- **AND** the browser document identifies its language as German

#### Scenario: Fall back on an unsupported device language

- **WHEN** there is no saved choice and none of the preferred languages is supported
- **THEN** the planner opens in English
