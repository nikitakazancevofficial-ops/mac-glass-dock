# Help shape Mac Glass Dock · Помоги улучшить Mac Glass Dock

**English and Russian are equally welcome. Пиши по-русски или по-английски.**

## Feedback / Отзывы

[Open an issue](https://github.com/nikitakazancevofficial-ops/mac-glass-dock/issues/new/choose) to share feedback, suggest a feature, report a bug or request support for your setup. Successful installs are useful too. Please include your distro, Plasma/KWin/Qt versions, GPU/driver, session type and monitor scale. `./scripts/system-info.sh` collects basic information locally; review it before sharing.

[Создай issue](https://github.com/nikitakazancevofficial-ops/mac-glass-dock/issues/new/choose): отзыв, идея, ошибка, запрос совместимости или успешный запуск. Укажи систему, версии Plasma/KWin/Qt, видеокарту/драйвер, сеанс и масштаб монитора. `./scripts/system-info.sh` собирает основные сведения локально; проверь их перед отправкой.

Use a clear title, explain what you expected and what happened, and add steps to reproduce. Keep passwords, tokens, private network names and personal files out of public logs/screenshots. There is no paid support requirement or guaranteed response time.

Напиши понятный заголовок, что ожидалось и что произошло, добавь шаги воспроизведения. Убери пароли, токены, имена частных сетей и личные файлы из публичных логов/скриншотов. Платной поддержки и гарантированного срока ответа нет.

## Code / Код

Discuss a substantial change in an issue first. Keep pull requests focused; describe behaviour and relevant validation. Do not submit copied code unless its license is compatible and its provenance is documented. Intentionally submitted contributions must be yours to submit and available under the [project's personal noncommercial license](LICENSE).

Крупное изменение лучше сначала обсудить в issue. Делай PR с одной понятной задачей, описанием поведения и проверок. Не добавляй чужой код без совместимой лицензии и указания происхождения. Отправляя вклад, ты подтверждаешь право на него и согласие на [лицензию проекта](LICENSE).

```bash
./scripts/build.sh --tests
ctest --test-dir build --output-on-failure
```

Tests use isolated XDG paths. GPU/compositor behaviour must also be tested in an isolated nested KWin session; offscreen success is not evidence of live refraction. Do not install QA capture modules into your daily compositor. See [development notes](docs/DEVELOPMENT.ru.md), [architecture](docs/ARCHITECTURE.md) and [refraction](docs/REFRACTION.md).

Тесты используют отдельные XDG-каталоги. Композитор и GPU проверяются дополнительно во вложенном изолированном KWin; offscreen-тест не доказывает живое преломление. QA-модули с захватом не устанавливай в основной композитор.

## Small ways to help / Помощь без кода

- Report your system and what works. / Расскажи о своей системе и том, что работает.
- Suggest clearer instructions or translations. / Предложи улучшения инструкции или перевода.
- Share a screenshot after removing personal details. / Покажи скриншот без личных данных.
- Star or share the repository if you want to. / По желанию поставь звёздочку или поделись ссылкой.
