from pathlib import Path
import zipfile

# Все пути — рядом со скриптом (на GitHub Actions это корень репозитория)
base = Path(__file__).resolve().parent
folder = base / "SpawnProbe"
folder.mkdir(parents=True, exist_ok=True)

hook_text = """// SpawnProbe — тест событий создания объектов для GoreBox 15.16.5.
// Проверяет, передаёт ли старая система модов имена новых объектов.
//
// ВАЖНО: это диагностический мод, не ESP.
// Он только выводит сведения о событии создания объекта в консоль.

ListenForInstantiation

OnInstantiate = function(args)
    print "=== NEW OBJECT CREATED ==="
    print "Spawner: " + args.player
    print "Object: " + args.name
end function

// Обязательный цикл обработки событий.
while true
    while _events.len > 0
        _nextEvent = _events.pull
        _nextEvent.invoke(_nextEvent.args)
    end while
    yield
end while
"""

info_text = """name:SpawnProbe
description:Logs object creation events for API testing.
version:1.0
active:true
"""

readme_text = """SpawnProbe — тестовый MiniScript-мод для GoreBox 15.x

Файлы:
- hook.txt — обработчик ListenForInstantiation
- info.txt — сведения о моде

Что делает:
При событии создания объекта выводит в консоль:
- args.player — имя создателя события
- args.name — имя созданного объекта

Это не ESP и не рисует контуры. Тест нужен, чтобы проверить,
какую информацию старая система модов передаёт при создании объектов.

Установка:
Скопируй папку SpawnProbe целиком в папку Mods игры.
Затем включи мод в игре и открой консоль модов.
Создай/заспавни тестовый объект и посмотри, появляются ли строки
NEW OBJECT CREATED, Spawner и Object.

Если мод выдаст ошибку, пришли точный текст ошибки и версию игры.
"""

(folder / "hook.txt").write_text(hook_text, encoding="utf-8")
(folder / "info.txt").write_text(info_text, encoding="utf-8")
(folder / "README.txt").write_text(readme_text, encoding="utf-8")

zip_path = base / "SpawnProbe_GoreBox15.zip"
with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as archive:
    for file_path in sorted(folder.iterdir()):
        archive.write(file_path, f"SpawnProbe/{file_path.name}")

print(f"Создан архив: {zip_path}")
print(f"Состав: {', '.join(p.name for p in sorted(folder.iterdir()))}")
