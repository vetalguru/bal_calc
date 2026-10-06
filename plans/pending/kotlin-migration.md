# Переезд интерфейса с Qt на Kotlin (Compose Multiplatform)

Статус: **утверждён 2026-10-06**, в работе. Создан: 2026-10-06.
Дальше по дорожной карте (2026-10-06): (1) этот переезд → (2) все функции
конкурентов → (3) новый интерфейс. Поэтому здесь — **паритет с текущей
Qt-версией**, без новых функций и без редизайна.

## Решения (подтверждены 2026-10-06)

| # | Вопрос | Решение |
|---|---|---|
| D1 | Ядро | C++ (core, storage/sqlite_manager, applogic) остаётся; Kotlin вызывает его через JNI. 108 тестов и эталоны точности не меняются |
| D2 | Десктоп | Тоже Kotlin: Compose Multiplatform, один UI на Android, Windows, Linux. Qt-интерфейс удаляется после паритета |
| D3 | UI | Compose + Material 3 |
| D4 | Обновление | Тот же applicationId `org.vetalguru.balcalc` и ключ пользователя: ставится поверх Qt-версии, база подхватывается |

## Архитектура

```
core / storage / applogic  (C++17, без изменений)
        │
bridge/   — C++: JSON-фасад над applogic (то, что сейчас делает Qt Backend:
            выбор оружия/патрона, условия сессии, решение, таблица, формы,
            библиотека, журнал, импорт/экспорт). Чистый C++, gtest.
        │
jni/      — C++: 3–4 функции JNI (open, call(method, json) → json, close).
        │
kmp/      — Gradle: Kotlin Multiplatform
  shared/   commonMain: модели (kotlinx.serialization), BalCalcApi (expect),
            ViewModel-состояние, все экраны Compose, строки uk/ru/en
            androidMain: загрузка .so, filesDir, file pickers, share
            desktopMain: загрузка .dll/.so из ресурсов, AppData-путь
  androidApp/  manifest, иконки, подпись, externalNativeBuild (CMake)
  desktopApp/  main(), jpackage (MSI, DEB), задача сборки native-lib
```

Почему JSON-фасад, а не JNI на каждый метод: граница — одна функция, вся
логика (и её тесты) в C++, Kotlin — тонкий клиент. Сейчас эта логика размазана
по `app/backend.cpp` (Qt) — переносится в `bridge` почти один в один.

## Где лежат данные (не меняются)

- Android: `/data/user/0/org.vetalguru.balcalc/files/balcalc.db` (= `filesDir`) — проверено на эмуляторе.
- Windows: `%APPDATA%\vetalguru\BalCalc\balcalc.db`; Linux: `~/.local/share/vetalguru/BalCalc/balcalc.db`.
- Стартовая библиотека (`data/seed`) — в assets / ресурсах, передаётся в `SeedLibrary`.

## Допущения (на подтверждение)

| # | Допущение |
|---|---|
| A1 | Паритет = все текущие экраны и функции: решение (с сеткой и выносом), таблица + график, условия, оружие/патроны (+ заводские), библиотека пуль (редактор, полосы BC, импорт .ammo/.drg/.reticle/.json), журнал и уточнение, настройки (единицы, язык, режим выноса), экспорт/импорт (файл, буфер). Внешний вид — близкий к текущему, Material 3; редизайн — этап 3 |
| A2 | Версии: последние стабильные на момент старта — Kotlin 2.x, Compose Multiplatform 1.12, AGP 9.2, Gradle wrapper; compileSdk 37, targetSdk 35 (как сейчас), minSdk 28 (как сейчас). Всё закреплено в `libs.versions.toml` |
| A3 | versionCode Android = 2 (у Qt было 1), versionName берётся из `project(VERSION)` |
| A4 | Десктоп-пакеты: jpackage MSI (Windows) и DEB (Ubuntu) + переносной ZIP; NSIS и Qt-деплой уходят. Свой JRE внутри пакета (jlink) |
| A5 | Qt-код (`app/`, `tests/ui`, Qt-часть CI, Qt-пресеты) удаляется в последней фазе, после паритета и проверки на телефоне. `bal-cli` остаётся |
| A6 | Тесты: C++ gtest для `bridge` (все методы) + Kotlin unit-тесты моделей + Compose UI-тесты ключевого сценария (десктоп, headless) — замена `tst_flow.qml`. Принцип прежний: всё зелёное на 3 платформах в CI |
| A7 | Подпись: `tools/android-release.ps1` переводится на Gradle (`signingConfig` из того же `%APPDATA%\BalCalc\android-signing.json`), ключ тот же |

## Never (вне объёма)

- Новые функции и редизайн (этапы 2 и 3).
- Изменения физики, схемы БД, sqlite_manager.
- iOS / web (KMP позволит позже, но не сейчас).

## Нужно установить (пользователь)

- Android SDK: `cmake;3.31.6` (через sdkmanager) — для сборки C++ под Android из Gradle.
- WSL (сборка и тесты Linux-десктопа): `openjdk-21-jdk fakeroot`.
- Остальное (Gradle, Kotlin, Compose) скачивает Gradle wrapper сам.

## Фазы (risk-first)

### 1. bridge — S/M
- `bridge/`: `Api` с методами Backend'а, вход/выход JSON (nlohmann), состояние
  сессии в БД как сейчас. Перенос логики из `app/backend.cpp`.
- Проверка: gtest на каждый метод (выбор, решение, таблица, формы, импорт,
  журнал/уточнение, ошибки); `ctest` зелёный в WSL.

### 2. Скелет KMP + JNI на трёх платформах — M (главный риск)
- `jni/`, `kmp/` (shared, androidApp, desktopApp); native-lib собирается
  CMake'ом из Gradle (Android: externalNativeBuild; десктоп: задача Gradle).
- Один экран: выбор примера → поправка на 300 м.
- Проверка: APK на эмуляторе показывает 1.61 MRAD; десктоп-окно на Windows
  и в WSL (offscreen-скриншот) — то же число.

### 3. Экраны: решение, условия, таблица/график — M
- Проверка: Compose UI-тест (решение, таблица 11 строк, совпадение с C++).

### 4. Экраны: оружие/патроны, библиотека пуль, журнал/уточнение, настройки, сетка — M
- Проверка: Compose UI-тест сценария из `tst_flow.qml` целиком.

### 5. Локализация, иконки, обновление поверх Qt — S
- Строки uk/ru/en из `.ts` → ресурсы; иконки; versionCode 2.
- Проверка: Qt-APK с данными на эмуляторе → установка Kotlin-APK поверх →
  оружие, патроны, журнал на месте.

### 6. Пакеты, подпись, CI, удаление Qt — M
- jpackage MSI/DEB/ZIP; Gradle-подпись; CI: C++ тесты + Gradle (Android APK,
  десктоп Windows/Linux, UI-тесты); удаление Qt; README/docs.
- Проверка: зелёный CI на 3 платформах; подписанный APK у пользователя на
  телефоне поверх Qt-версии.

## Прогресс

| Фаза | Статус |
|---|---|
| 1 bridge | ✅ `bridge/` (JSON-фасад, 33 метода), 11 тестов gtest; ctest 119/119 |
| 2 Скелет KMP + JNI | ✅ jni/, kmp/ (shared, androidApp, desktopApp); 1.61 MRAD на эмуляторе (поверх Qt-версии, её база подхвачена), Windows и Linux (UI-тест Compose); CI-задача Kotlin |
| 3 Решение, условия, таблица | ✅ навигация (панель снизу / сбоку), «Решение», «Условия», «Таблица» + график; строки в ресурсах Compose; UI-тест (телефон и десктоп), эмулятор |
| 4 Остальные экраны | ✅ 4a (#28): оружие и патроны, редакторы, заводские, обмен; 4b: библиотека пуль (редактор, полосы BC, импорт), журнал и уточнение, сдвиг СТП, сетка (4 сетки стартовой базы), настройки; UI-тесты (6) |
| 5 Локализация, обновление | ✅ uk/ru из .ts (266 строк), сообщения ядра, смена языка из настроек; эмулятор поверх Qt-версии: данные и язык подхвачены |
| 6 Пакеты, CI, удаление Qt | 🔄 6a: подпись через Gradle, MSI/DEB/ZIP, CI-задачи Kotlin на Linux и Windows, версия 0.2.0; 6b (удаление Qt) — после проверки на телефоне |
