# Validierung 1.3

## Umfang

Verbesserungen aus dem unabhängigen technischen Audit von 1.2. Alle Tests verwenden die reale Engine. Es wurden keine tatsächlichen Hörtests und keine Studio-One-Pro-8-Sitzung durchgeführt. Aussagen über Natürlichkeit sind deshalb keine bestätigten Hörergebnisse.

## Regressionen

- Prozenttexte, Dezimalkomma, „aus“ und Intensitätsnamen.
- Mono-Begrenzung bei maximalen gültigen Eigenstimmen-/Outputwerten, 44,1 und 48 kHz.
- Stereo-Eigenstimme und exakte gemeldete Verzögerung, auch im Host-Bypass.
- Generator auf einem deaktivierten Eingang, AN/AUS, gespeicherter Pegel und Tail-Meldung.
- Recall vor und nach Vorbereitung; gespeicherte manuelle Besetzung; Migration alter Zustände.
- Stabile Positionen über automatische Phrasenwechsel.
- Bestehende Tests für Sample-Rates, Blockteilung, Parameterautomation, ungültige Werte und Allokationen.

## Bewusste Grenzen

Das Plugin vervielfacht dieselbe Darbietung. Es erzeugt keine unabhängig gesprochenen Phoneme und keine garantierten Alters-/Geschlechtsidentitäten. Die Pitch-Engine bleibt ein Delay-Verfahren. Die begrenzte Zeitvariation beeinflusst auch die Tonhöhe.

Der Projektzustand speichert Parameter und Besetzung, keinen vollständigen Snapshot von Hallpuffern, Phrasenpositionen oder laufenden Gemurmel-Abspielköpfen. Ein neuer Lauf aus gleichem Startzustand ist reproduzierbar; beliebige Transportverläufe müssen nicht identisch klingen.

Das Gemurmel besitzt 14 unveränderte 30-Sekunden-Aufnahmen (4/4/1/3/2). Zufällige Ausschnitte vermeiden einen festen Loop, schließen wiedererkennbare Einzelereignisse aber nicht aus. Es wurde kein zusätzliches Audiomaterial mit unklarer Herkunft eingebaut.

Eine WSOLA-/Formant- oder neuronale Alternative wurde nicht ohne echte Sprach-Hörvergleiche zum neuen Standard erklärt. Priorität hatten nachgewiesene Fehler und kontrollierte Verbesserungen innerhalb der bestehenden Bedienung.

## Reproduktion

```sh
cmake -S . -B build/dsp -DAUDIO_DSP_ONLY=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build/dsp --parallel 2
ctest --test-dir build/dsp --output-on-failure
```

Vollständige Prozessor-, Editor- und VST3-Prüfung: Windows-Workflow `build.yml`; pluginval Strictness 8. Release nur nach erfolgreicher Prüfung und ausdrücklicher Freigabe über den bestehenden manuellen Workflow.

## Ausgeführte Prüfungen

Die endgültigen Ergebnisse werden vor Veröffentlichung ergänzt.

## Expliziter Release-Auftrag per Commit

Neben dem manuellen Workflow kann ein ausdrücklich freigegebener Push auf `main` mit der ersten Commitzeile `release: Tumult <VERSION> <Beschreibung>` die Veröffentlichung beauftragen. Die Nummer muss exakt zur Datei `VERSION` passen. Normale Pushes veröffentlichen weiterhin nichts. Der Release-Auftrag durchläuft dieselben Versions-, Screenshot-, Build-, Regressionstest- und pluginval-Prüfungen wie der manuelle publish-Lauf. Diese Alternative ermöglicht eine vom Auftraggeber autorisierte Veröffentlichung über den GitHub-Connector ohne zusätzliche Browseranmeldung.
