# Отчёт — этап 4

Дата: 20 октября 2026 года. Проект: /home/nikita/Projects/mac-glass-dock.

## 1. Итог разработки

Продолжена рабочая основа этапа 3, а не создано новое приложение. Добавлены общий GPU-материал, адаптивные иконки, каталог приложений, собственные стеклянные панели и настройки, необязательная интеграция окон KWin, автоскрытие и диагностика.

Дополнительное исправление по сообщению пользователя: Gaussian magnification теперь охватывает весь ряд, включая левую кнопку приложений, правую кнопку настроек и первое/последнее закреплённое приложение. У управляющих кнопок больше нет фиксированного масштаба. Их увеличенные прямоугольники также входят в native input region. Координата указателя переводится в базовую систему относительно центра только по событию движения: изменение ширины окна само не меняет эту координату и не создаёт обратную связь масштаба.

Третья картинка пользователя принята за основной материальный референс. Уменьшены плотность центра и яркость отражений, удалены зернистость и яркое внутреннее кольцо, уменьшена оптическая фаска маленьких кнопок. Это собственная художественная модель объёмной кромки с настоящим системным blur, **не идентичная macOS и не рефракция рабочего стола**.

Git в исходном каталоге не был инициализирован. Рабочие исходники до изменений сохранены локально в backups/stage3-source-20261020.tar.gz. Коммит/публикация не выполнялись.

## 2. Полный список изменений

Изменённые исходные файлы:

- CMakeLists.txt, .gitignore.
- src/main.cpp.
- src/dockplatform.h, src/dockplatform.cpp.
- src/docksettings.h, src/docksettings.cpp.
- src/applicationmodel.h, src/applicationmodel.cpp.
- qml/Main.qml.
- qml/components/Dock.qml, qml/components/DockItem.qml.
- qml/effects/GlassSurface.qml.
- shaders/glass.frag.
- tests/docktests.cpp, tests/settingstests.cpp.
- config/settings.example.ini.
- README.md, docs/ARCHITECTURE.md, docs/LIQUID_GLASS.md, docs/ROADMAP.md.

Новые файлы:

- src/applicationcatalogue.h, src/applicationcatalogue.cpp.
- src/windowtracker.h, src/windowtracker.cpp.
- src/framediagnostics.h, src/framediagnostics.cpp.
- qml/effects/VolumetricGlassSurface.qml.
- qml/components/AdaptiveAppIcon.qml.
- qml/components/GlassPanelWindow.qml.
- qml/components/GlassButton.qml, qml/components/GlassSlider.qml, qml/components/GlassSwitch.qml.
- qml/LauncherWindow.qml, qml/SettingsWindow.qml, qml/AppMenuWindow.qml.
- shaders/adaptive-icon.frag.
- tests/stage4tests.h, tests/stage4tests.cpp, tests/windowprobe.cpp.
- docs/KWIN_STAGE4_RESEARCH.md, docs/AGENT_STAGE4_REPORT.md.

Дополнительно: локальная резервная копия, автоматически генерируемые build/build-release, QSB, журналы и QA-кадры внутри build. Исторические отчёты этапов 1/3 сохраняются; их прежние результаты не являются результатами текущей версии.

## 3. Архитектура Glass Engine

VolumetricGlassSurface — общий компонент для Dock, поиска, кнопок, панелей, меню и подсказок. GlassSurface оставлен совместимым адаптером DockPlatform. Основные параметры: геометрия, радиус, плотность, opticScale, указатель, тема, толщина, отражение, блики и tint.

Корневые поверхности используют фактическую доступность blur. Вложенные компоненты находятся внутри общей скруглённой области размытия окна. При отсутствии/отключении blur прозрачность слегка уменьшается для читаемости. Фон не подменяется картинкой.

LayerShellQt размещает Dock снизу, панели — отдельными Overlay surfaces. Exclusive zone равна 0, системная панель остаётся на месте. Dock не получает keyboard focus, панели запрашивают OnDemand. Вне закруглённой поверхности и реальных иконок ввод не перехватывается.

Все native регионы задаются в логических координатах; дополнительного ошибочного умножения на DPR нет. Панели и Dock включают поля для теней.

## 4. Шейдеры

**glass.frag → qrc:/effects/glass.frag.qsb**

GLSL 440 / Qt ShaderTools. SDF скруглённого прямоугольника, derivative anti-aliasing, профиль толщины, нормали фаски, Fresnel-like отражение, направленный контур, локальный свет указателя, очень деликатный цветовой сдвиг, внутренняя и аналитическая внешняя тень. Один fragment pass, premultiplied alpha. Нет time uniform, зерна, desktop sampler или экранного захвата.

**adaptive-icon.frag → qrc:/effects/adaptive-icon.frag.qsb**

Superellipse-like контейнер, слабая тонировка и выборка полной оригинальной иконки в padded square. Логотип не обрезается маской контейнера. Явно symbolic-иконки тонируются на копии изображения. Системные файлы не изменяются.

Software/GPU-off fallback использует общий QML-градиент с контурами. Он работоспособен, но качество и оптика не идентичны GPU.

## 5. Отличия от старого стекла

Раньше материал применялся преимущественно к Dock. Теперь одна система используется во всех собственных панелях и контролах. Вместо шумной плоской заливки — SDF-фаска, нормали и управляемый свет. Основная плотность Crystal Clear снижена с прежних 0.40 до 0.16. Существующие явные пользовательские параметры не перезаписываются молча: новый пресет можно выбрать в панели.

Маленькие капсулы не получают ту же широкую фаску, что большая панель: opticScale уменьшает её. Белая тяжёлая двойная обводка не является основным GPU-дизайном. Для текста добавлена тонкая контрастная обводка; автоматическая тема следует системной теме, а не яркости конкретного участка обоев.

## 6. Application Launcher

Каталог KService / KSycoca, исключение NoDisplay и неподходящего desktop environment, локализованная сортировка QCollator. Сетка, поиск по названию/generic name/keywords/desktop ID, категории, прокрутка, закрепление/открепление и запуск через KIO.

Добавлены кнопка каталога в Dock, настройка из каталога, Escape и закрытие по потере фокуса, если композитор её сообщает. Нет полноэкранного прозрачного перехвата кликов. Клиент Wayland не может гарантировать событие любого внешнего клика.

Ошибки запуска показаны в собственной панели. Desktop entry разрешается заново перед запуском; строки не превращаются в shell-команды.

## 7. Управление окнами

Необязательная библиотека KWayland, KDE org_kde_plasma_window_management. По умолчанию выключено. Отображаются подтверждённые окна, count/active/minimized, работающие незакреплённые приложения. Группировка по app ID и StartupWMClass, один desktop ID на приложение.

Поддержаны выбор/активация, восстановление, перебор окон группы, запрос минимизации и закрытия с проверкой capability и локального token. Контекстное меню использует тот же материал. «Открыть новое окно» повторно запускает desktop entry; single-instance программа может переиспользовать существующее окно.

CLI --enable-window-tracking включает интеграцию на текущий запуск, не записывая сам флаг в INI. Переключатель в настройках сохраняется отдельно. Неизвестные app ID, новые политики KWin и переподключение после рестарта композитора остаются рисками совместимости.

## 8. Анимации

Непрерывная Gaussian-функция всего ряда, SpringAnimation фактической ширины ячеек, настраиваемые damping/speed. Row исключает перекрытие иконок. Пресс-сжатие, конечное подпрыгивание на pending launch, настоящее running-состояние отдельно от pending-индикатора. Каталог и панели плавно меняют opacity/scale, без бесконечного отскока.

Автоскрытие: задержка, сдвиг/затухание, затем уменьшение native окна до 3 логических px. Hover возвращает Dock. При открытых панелях скрытие приостановлено. Preview намеренно отключает автоскрытие. При существующей панели Plasma полоса может располагаться на границе доступной области над панелью, а не на физическом краю экрана.

Reduced Motion отключает увеличение/подпрыгивание и сокращает переходы. Не каждая микрореакция имеет отдельный глобальный motion-controller.

## 9. Результаты тестирования

Итоговые проверки текущего кода:

| Проверка | Результат | Журнал в build/ |
| --- | --- | --- |
| Debug, ./scripts/build.sh | Успех, exit 0 | stage4-debug-final.log |
| Release, BUILD_TESTING=OFF | Успех, exit 0 | stage4-release-final.log |
| Release QML smoke | Успех, exit 0; Qt6Test не связан | stage4-release-smoke.log |
| CTest | 5/5, 0 ошибок | stage4-ctest-final.log |
| QML lint | exit 0, без warnings/errors/unused-import info | stage4-qmllint-final.log |
| Native Dock, 3 последовательных повтора | Каждый: 7 passed, 0 failed | stage4-edge-repeat-1.log, stage4-edge-repeat-2.log, stage4-edge-repeat-3.log |
| Native Stage4 | 9 passed, 0 failed, без skip | stage4-native.log |
| Native Dock, QT_SCALE_FACTOR=1.5 | 7 passed, 0 failed | stage4-interaction-150.log |
| Native Stage4, QT_SCALE_FACTOR=1.5 | 9 passed, 0 failed, без skip | stage4-native-150.log |
| Native preview | 7 passed, 0 failed | stage4-preview.log |
| QSB dump обоих ресурсов | Валидные fragment packages: glass 6918 B, icon 3317 B; по 6 backend variants | stage4-glass-qsb.log, stage4-icon-qsb.log |
| Обычный запуск и открытие launcher | Успех, собственный кадр сохранён | stage4-launcher-default.png |
| Wayland trace | layer-shell, exclusive zone 0, input/blur regions Dock и панели | stage4-wayland-final.log |

В offscreen Stage4: 8 passed / 1 expected skip (реальные Wayland-окна); в native этот сценарий выполнен полностью. Негативный smoke проверяет ошибочный QML-модуль и ненулевой выход. Увеличение проверено для центра, соседей, обеих крайних кнопок и первого/последнего приложения; проверены отсутствие перекрытий, input mask, alpha кадра, KIO-клик с безопасным launch-probe и чистое закрытие. Настройки сохранялись только в тестовый XDG и восстанавливались.

Проверка 150% выполнена через QT_SCALE_FACTOR=1.5, без изменения глобального масштаба KDE. Кадры Dock: 536×203 при 100%, 804×305 при 150%. Это не тест физического hotplug или набора мониторов с разным DPR.

В раннем native-прогоне был abort теста при first() пустого списка окон, вызванный гонкой настройки; он устранён. Повторная проверка выявила также нестабильность масштаба крайней кнопки из-за привязки координат к изменяющейся ширине ряда. После перехода к фиксации базовой координаты на pointer events выполнены три последовательных native-прогона наведения (7/7 каждый), затем 150% и preview. Старый stage4-interaction-native.log сохраняет неудачный прогон ДО этой правки и не является итоговым результатом. Итоговые журналы из таблицы не содержат падений, QML load errors, ShaderEffect errors или binding loops.

QA-кадры: build/stage4-dock-final-hover.png, build/stage4-native-launcher-contrast.png, build/stage4-native-settings-contrast.png, build/stage4-native-light-contrast.png и build/stage4-150-settings.png. Исходные alpha-кадры сохранены рядом. Они осмотрены; сходство материала частичное, физическая идентичность референсу не заявляется.

Доступ к реальному KDE Wayland есть. Выполнены настоящие native-запуски, QTest-события, framebuffer-снимки собственных окон и проверка собственного helper-окна. Снимки не содержат итоговый compositor blur вместе с обоями. QA-файлы *-contrast.png — композиция собственного alpha-кадра на двух тестовых градиентах, **только для анализа**, не runtime background.

Обнаруженные и исправленные проблемы: фиксированный масштаб крайних контролов; отсутствие их увеличенной геометрии в input region; пересоздание делегатов на каждую смену active/title; callbacks запуска по изменяемому индексу; null-parent anchors при reset модели; невалидная комбинация Qt flags; использование закрытой перегрузки KService; гонка отложенного сохранения с временным window tracking в тесте; небезопасное first() в polling теста; слишком белое двойное кольцо; слабый контраст текста.

Qt 6.12 может оставлять per-instance ShaderEffect.Uncompiled при попадании в общий shader-info cache. Это сверено с [исходником Qt](https://github.com/qt/qtdeclarative/blob/v6.12.0/src/quick/items/qquickshadereffect.cpp): cache-hit обходит prepareShaderCode нового manager. Тест не выдаёт один enum за доказательство рисунка: проверяет отсутствие Error, полупрозрачный кадр и отличие GPU/fallback. Тест Dock проверяет действительный QSB URL, наличие ресурса и alpha нарисованной поверхности вместо ненадёжного требования Compiled для каждого экземпляра.

## 10. Оптимизация

Нет постоянного material timer и time uniform. Один pass на поверхность, без копирования полноэкранного framebuffer клиентом. KWin выполняет своё размытие. Текстуры иконок запрашиваются с фиксированным разрешением и DPR, а не перерисовываются provider на каждый шаг масштаба. Virtualized GridView ограничивает число делегатов.

Изменения окон объединяются таймером 30 мс; неизменный состав модели получает dataChanged вместо reset. Настройки применяются сразу, запись объединяется 250 мс. FrameDiagnostics слушает frameSwapped и не запрашивает дополнительные кадры.

Release diagnostic run: за первые 5 с Dock выдал 3 кадра, каждая скрытая панель — 0; OpenGL (graphicsApi=3), DPR=1. Активные интервалы в этом коротком стартовом замере: средний 10.98 мс, максимум 19.49 мс. Это не замер устойчивого hover FPS. В native interaction idle-окне 250 мс отмечен 1 кадр (остаток переходов), порог теста <8.

Снимок ps на первой секунде Release: RSS 159068 KiB (около 155 MiB, включая Qt/драйвер/каталог). Bash time за ограниченные 7 с: user 0.275 с, sys 0.129 с. Диагностический процесс намеренно остановлен timeout (exit 124), это не сбой загрузки. /usr/bin/time отсутствует; пакет не устанавливался, peak RSS не измерен.

Это не GPU timestamp profiling, не гарантия 60 FPS на любом железе и не доказательство отсутствия утечек при многочасовой работе.

## 11. Ограничения Wayland

Blur protocol не отдаёт клиенту текстуру фона. Настоящая рефракция и отражение чужих окон недоступны обычному ShaderEffect. Ни portal screen capture, ни обход ограничений не используются.

KDE window protocol специфичен для композитора. Мультиоконность реализована, но проверка большого числа разнородных приложений ещё нужна. Выбор экрана и fallback на primary реализованы, физический multi-monitor/hotplug не сертифицирован. HDR/ICC и другие композиторы не сертифицированы.

Нельзя из Dock универсально изменить форму CSD сторонних окон. Системную анимацию минимизации выбирает KWin; в текущей сессии Magic Lamp supported, но loaded=false. Blur loaded=true. Эффекты не переключались. Геометрия конечной точки системной анимации сворачивания в наш Dock не регистрируется; при параллельной панели Plasma нельзя обещать сворачивание визуально именно в нашу иконку.

## 12. Действия пользователя

Установка пакетов в этом этапе не потребовалась. Проверенная система: Qt 6.12.0, GCC 16.2.1, CMake 4.4.4, KWin/LayerShellQt/KWayland 6.8.0, KF6 6.31.0.

Зависимости Arch/CachyOS: cmake, gcc, make, qt6-base, qt6-declarative, qt6-shadertools, qt6-svg, qt6-wayland, layer-shell-qt, kwindowsystem, kservice, kio, breeze-icons; необязательно kwayland. Qt Test требуется только для тестовой сборки.

Из каталога проекта:

```bash
./scripts/build.sh
ctest --test-dir build --output-on-failure
./build/mac-glass-dock
./build/mac-glass-dock --settings
./build/mac-glass-dock --launcher
./build/mac-glass-dock --preview
```

Не запускайте несколько копий Dock одновременно для визуальной оценки. Завершение — «Выход» в контекстном меню либо Ctrl+C из терминала. Настройки только собственные XDG: ~/.config/mac-glass-dock/settings.ini. Если сохранена старая плотность, выберите Crystal Clear.

Release:

```bash
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build-release --parallel 2
./build-release/mac-glass-dock
```

Опционально --enable-window-tracking / --diagnostics. Отдельные инструкции native-тестов находятся в README. Не запускайте их одновременно с CTest: используется одна изолированная тестовая конфигурация.

## 13. Нереализованные функции и статусы

Статус Fully Working означает проверку в текущем окружении, а не универсальную сертификацию.

| Функция | Статус | Граница |
| --- | --- | --- |
| Загрузка QML через модуль / пять закреплений | Fully Working | Зависит от установленных desktop entries |
| Dock layer-shell / прозрачность / маска ввода | Fully Working | Текущий KDE Wayland |
| Увеличение всего ряда, включая края | Fully Working | Проверяется тестом наведения |
| Каталог, поиск, категории, pins | Fully Working | KService/KSycoca |
| Безопасный запуск KIO и обработка ошибок | Fully Working | Запуск не равен появлению окна |
| Настройки / темы / presets / XDG | Fully Working | Автотема не анализирует обои |
| Общий GPU-материал и QML fallback | Fully Working | Разница качества fallback документирована |
| Совпадение с третьим референсом | Partially Working | Нет desktop refraction; художественная аппроксимация |
| Blur за поверхностью | Partially Working | Протокол/регион проверены, итог на любых обоях не сертифицирован |
| Автоскрытие | Partially Working | Есть ограничение физического края при панели Plasma |
| KWin tracking / activate / minimize / close | Experimental | Реальные действия helper-окна; KDE-specific opt-in |
| Группы нескольких окон разных приложений | Partially Working | Код есть, расширенный ручной сценарий нужен |
| Multi-monitor / hotplug | Partially Working | Реализовано, физически не проверено |
| Desktop refraction / отражение окружения | Not Implemented | Нужен разрешённый compositor-side источник |
| Универсальные скругления сторонних окон | Not Implemented | Сторонние программы не меняются |
| Собственный Genie / KWin effect plugin | Not Implemented | Системный Magic Lamp не включался |
| 60 FPS на любом GPU / длительный leak audit | Not Implemented | Нужен отдельный profiling matrix |

## 14. Что ещё необходимо для полного Liquid Glass

Нужен отдельный opt-in модуль KWin с разрешённым underlay в нужной точке стека, правильной деформацией реальной текстуры фона, damage tracking, HDR/linear-light и дробным масштабом. Нужны согласование surface namespace, исключение рекурсии собственного Dock и строгий lifecycle GPU-ресурсов. Подробная концепция: KWIN_STAGE4_RESEARCH.md.

Далее: визуальные compositor-снимки на реальных светлых/тёмных обоях, настройка фаски/света по ним, GPU profiling, тест нескольких физических мониторов, Flatpak/Chromium/несколько окон, KDE protocol lifecycle, accessibility и более полная клавиатурная навигация.

Панели Plasma, kwinrc, глобальная тема, оригинальные иконки и сторонние desktop entries не менялись. Контрольные суммы конфигураций Plasma/KWin и выборочных системных ресурсов совпали с исходными:

| Файл | SHA-256 до и после |
| --- | --- |
| ~/.config/plasma-org.kde.plasma.desktop-appletsrc | b6563b575f5031dd799a1c3436c8de247e65ddadde46abec1a5e4ae53564138e |
| ~/.config/kwinrc | 5d3e4b2156692417827f3af3d0b30830f71b2b86e788ead39ceec55f8c31ee44 |
| /usr/share/icons/breeze/index.theme | 6f0371bf65939ec6438d5a46ba0ce370548a16a3164e298b36dc7c182c66b6c7 |
| /usr/share/applications/org.kde.dolphin.desktop | 2b377209f87cef86def4a3122b8cc3d63911fb3c99db9ef99ebea48f53b32d54 |
| /usr/share/icons/breeze/apps/48/system-run.svg | 76a4e84cf6967409b83445fd3244cecb584177db1ce29148e31a8034875a5bca |

Автозапуск, sudo, системная установка, перезапуск KWin и публикация не выполнялись. Собственные native-тестовые окна и helper-процессы после проверки закрыты.
