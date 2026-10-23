# Отчёт проверки и настройки оптики

Обновление от 21 октября: исправления задержки, HiDPI, hover и анимаций описаны в [AGENT_REFRESH_REPORT.md](AGENT_REFRESH_REPORT.md). Ниже сохранены результаты исходной проверки.

Дата: 20 октября 2026 года. CachyOS, KDE Plasma Wayland, Qt 6.12.0, GCC 16.2.1, CMake 4.4.4, KWin 6.8.0.

## Изменения

- Профиль «Со скринов»: SurfaceOpacity 0.04, Thickness 24, EdgeReflection 1.0, HighlightIntensity 0.84, IconSize 56, MaxScale 1.50, AnimationSpeed 2.55, SpringDamping 0.46.
- Исправлено увеличение крайних иконок: их увеличенная геометрия участвует в layout и input region.
- Добавлен пользовательский KWin Effect с edge distortion, слабым blur и отдельными icon lenses.
- Добавлен fallback и watchdog при выгрузке или ошибке эффекта.
- Системные панели, kwinrc и автозапуск не менялись.

## Результаты

- `./scripts/build.sh` — успешно.
- `ctest --test-dir build --output-on-failure` — 6/6 успешно.
- Native GPU optics — 4/4, включая edge warp и square icon lens.
- Nested KWin session — 6/6; `ready=true`, `draws=24`, `iconDraws=21`.
- Production-сеанс: `windows=2`, `ready=true`; после усиления профиля `draws=53214`, `iconDraws=22642`, `refractionStrength=20`, `iconRefractionStrength=17`.
- `ldd -r` production `.so` не выявил undefined/not found.

QA-снимки `build/optics-nested-composed2-*.png` подтверждают изменение сетки по краям и дополнительные линзы под иконками. Это не снимок рабочего стола пользователя.

## Ограничения

Полная идентичность macOS не заявляется. Преломление доступно только для собственных окон Dock; произвольный desktop capture и обход Wayland не используются. HDR, multi-monitor с разными DPR и сторонние compositor effects требуют дальнейшей проверки.

## Запуск

```bash
./build/mac-glass-dock --settings
./build/mac-glass-dock --launcher
./build/mac-glass-dock --preview
```

Эффект загружается вручную только в текущий сеанс. Команды выгрузки находятся в [REFRACTION.md](REFRACTION.md).

## Что ещё нужно для полного Liquid Glass

Дальше нужны длительная проверка на разных GPU и обоях, оценка HDR/цветового пространства, более точная обработка transformed/fullscreen surfaces и отдельная интеграция с KWin для возможностей, которых не предоставляет публичный Wayland API.
