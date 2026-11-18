# Reproduzierbare Screenshots

Die PNGs werden aus der echten JUCE-Oberfläche erzeugt, einschließlich der aus `VERSION`
kompilierten Versionsanzeige und einer kurzen Sprechprobe, damit die Anzeige der Menge mit
leuchtenden Stimmen zu sehen ist. Keine nachträglich ersetzten Versionsziffern.

Am einfachsten über GitHub: Actions > **Screenshots erzeugen** > Run workflow. Der Lauf nimmt beide
Looks auf und checkt `docs/oberflaeche-dunkel.png`, `docs/oberflaeche-hell.png` und `docs/screenshots.json` ein.

Lokal:

```sh
cmake -S . -B build
cmake --build build --config Release --target TumultScreenshot
# Windows:
build/TumultScreenshot_artefacts/Release/TumultScreenshot.exe <absoluter-Pfad-zum-docs-Ordner>
```

Unter Linux benötigt das Werkzeug eine X11-Sitzung (z. B. Xvfb); der ausführbare Name
hat dort kein `.exe`. Es nutzt die echte Message-Loop und schreibt Aufnahmen mit
zweifacher Pixelauflösung. Theme und Beispielparameter betreffen ausschließlich die
lokale Screenshot-Instanz, keine Plugin-Defaults oder gespeicherten Nutzerprojekte.

`docs/screenshots.json` enthält die einkompilierte Version und die erzeugten PNG-Dateien.
Diesen Eintrag nicht manuell hochsetzen. Vor Veröffentlichung prüft der Workflow die
Übereinstimmung mit `VERSION` und das Vorhandensein aller aufgeführten Bilder. Das Manifest
wird als Release-Asset angehängt, damit die Homepage Aufnahmen aus genau dem freigegebenen
Release-Tag laden kann.
