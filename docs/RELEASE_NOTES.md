# 0.4.0 — first public preview / первая публичная версия

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
