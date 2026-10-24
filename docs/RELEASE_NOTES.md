# 0.4.0 — first public preview / первая публичная версия

## 24 October 2026 — automatic startup and recovery / автозапуск и восстановление

Fixed automatic startup after entering KDE, including after reboot. The previous login service ran once and did not monitor Dock afterwards. A dedicated user systemd service now tracks the actual Dock process and restarts it after exits or crashes. A separate supervisor recreates Dock after sleep to reconnect its Wayland surfaces to the restored displays, and recovers an unresponsive Dock. Temporary KDE/display failures no longer disable automatic recovery. Disabling autostart keeps the current Dock open and removes its restart policy.

Исправлен автоматический запуск после входа в KDE, в том числе после перезагрузки. Прежняя служба выполнялась один раз и дальше не следила за Dock. Теперь отдельная пользовательская служба systemd отслеживает сам процесс и запускает его заново после завершения или сбоя. Наблюдатель пересоздаёт Dock после сна для подключения поверхностей Wayland к восстановленным экранам и восстанавливает неотвечающий Dock. Временные сбои KDE или экранов больше не отключают автоматическое восстановление. Отключение автозапуска оставляет текущий Dock открытым и убирает политику повторного запуска.

**Validation / Проверка:** 19 automated recovery tests passed. Automatic restart after forcibly terminating Dock and disabling/re-enabling autostart were verified in a live KDE session. The author confirmed successful startup after a real laptop reboot. Sleep detection/recovery is covered by automated tests; a physical suspend/resume cycle has not yet been verified.

**Проверка:** Все 19 автоматических тестов восстановления прошли. В живом сеансе KDE проверены повторный запуск после принудительного завершения Dock и отключение/включение автозапуска. Автор подтвердил успешный запуск после настоящей перезагрузки ноутбука. Обнаружение сна и восстановление проверены автоматическими тестами; физический цикл сна и пробуждения пока не проверялся.

**Existing installations / Для уже установленного Dock:** Run these commands from the project folder to update the source and replace the old startup configuration. Autostart remains optional; these commands enable it. / Выполни команды из папки проекта, чтобы обновить код и заменить прежнюю конфигурацию запуска. Автозапуск остаётся необязательным; эти команды его включают.

```bash
git pull --ff-only
python3 scripts/desktop-session.py enable-autostart
systemctl --user restart mac-glass-dock-login.service
```

## 23 October 2026 — volume control fix / исправление регулировки громкости

Fixed volume control in Control Center: the slider now correctly changes the system output volume. Dock previously passed a doubled percent sign to WirePlumber (`37%%` instead of `37%`), which caused the command to be rejected. Added regression tests for volume changes, the 0–100% limits, rapid slider updates and synchronization with an external mixer. The fix was also checked with the real PipeWire audio service. Update the source, rebuild and restart Dock to apply it.

Исправлена регулировка громкости звука в центре управления: ползунок теперь корректно меняет системную громкость. Раньше Dock передавал WirePlumber двойной знак процента (`37%%` вместо `37%`), из-за чего команда отклонялась. Добавлены регрессионные тесты изменения громкости, границ 0–100%, быстрых движений ползунка и синхронизации с внешним микшером. Исправление также проверено с настоящим звуковым сервисом PipeWire. Для применения обнови исходники, пересобери и перезапусти Dock.

## Preview overview / О предварительной версии

A glass dock for KDE Plasma on Wayland, with live compositor-assisted refraction, spring icon magnification, Genie transitions, an application launcher and a Control Center. Includes optional autostart, reversible panel integration, Arch installation helpers and English/Russian documentation.

Стеклянный Dock для KDE Plasma на Wayland: живое преломление через композитор, пружинное увеличение иконок, Genie-анимации, каталог приложений и центр управления. Есть необязательный автозапуск, обратимая интеграция панели, помощник установки для Arch и документация на русском/английском.

**Requirements:** full refraction is audited for KWin 6.8.0 with Qt 6.11+; other versions use basic native blur. Main Dock requires Qt 6.8+. This is an early preview; cross-GPU, HDR and multi-monitor validation remains ongoing.

**Требования:** полное преломление проверяется на KWin 6.8.0 с Qt 6.11+; для других версий предусмотрено базовое размытие. Самому Dock нужен Qt 6.8+. Проверки разных GPU, HDR и нескольких мониторов продолжаются.

Free for personal noncommercial use; commercial use is prohibited under the [license](../LICENSE). / Бесплатно для личного некоммерческого использования; коммерческое использование запрещено.

[Get started / Начать](../README.md) · [Русская инструкция](../README.ru.md) · [Feedback / Отзыв](https://github.com/nikitakazancevofficial-ops/mac-glass-dock/issues/new/choose)
