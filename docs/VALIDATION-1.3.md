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

Mono verwendet den Kanal-Mittelwert vor der Begrenzung. Das kann gegenüber 1.2 etwa 3 dB weniger Pegel bedeuten und verhindert den bisherigen Summenaufschlag.

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

- Lokale vollständige JUCE-Prozessortests einschließlich Editor-Erzeugung, Generator, State und Bypass: erfolgreich, CTest 1/1 (6,43 s).
- Lokaler Release-DSP-Build: erfolgreich, CTest 1/1 nach der Bypass-Ergänzung (175,85 s in einer parallel ausgelasteten Cloud-Umgebung).
- Linux-CI mit ASan/UBSan auf `7549245` einschließlich Bypass-Regression: erfolgreich.
- Erster vollständiger Windows-Lauf auf `7b74e29`: Build, DSP-/Prozessortests erfolgreich; die damalige pluginval-Ausführung war nicht vollständig abgewartet und zählt nicht als bestandene Validierung. Der endgültige Stand `7549245` einschließlich Bypass und Generator-Kategorie besteht Build und Regressionstests; sein pluginval-Protokoll zeigte jedoch einen Recall-Fehler bei fraktionalen Bool-Werten und wurde vom alten Workflow nicht zuverlässig ausgewertet: [Prüflauf](https://github.com/StromschlagProductions/Tumult-vst/actions/runs/44373300308).
- Externe Engine-Messung bei 44,1 und 48 kHz: nach zehn Sekunden Stille 0 ms Tempo-Akkumulation, nach einer Sekunde Sprache ungefähr 36,5 ms maximaler individueller Versatz. Mono-Spitze bei maximalen gültigen Verstärkungen höchstens 1,0 (0 dBFS).
- 14 echte Gemurmeldateien untersucht; neun feste Intensitätsstellungen jeweils 60 Sekunden gerendert. Bei festen Stufen bleiben die langfristigen Pegel gegenüber 1.2 praktisch unverändert; kleine DC-Anteile und geringe Links-Rechts-Korrelation. Keine Bestätigung der wahrgenommenen Natürlichkeit oder Unhörbarkeit wiederkehrender Ereignisse.
- Das vorhandene Demo-Werkzeug wurde vor/nach der Änderung mit seinem synthetischen Silbensignal ausgeführt. Ausgangs-RMS für Klein/Mittel/Groß/Stadion: vorher −30,9/−30,3/−30,6/−30,7 dBFS, danach −30,7/−30,8/−30,9/−30,7 dBFS. Eingangs-RMS jeweils −32,2 dBFS. Eigenstimme und Raum sind enthalten; keine isolierte LUFS-Messung echter Sprache.

### Rechenaufwand

Ein separater direkter Engine-Benchmark mit GCC 14.2, `-O2`, Xeon Platinum 8573C, 48 kHz, 512 Samples, zwölf Sekunden Dauerton, Raum 35 %, ohne Eigenstimme und Gemurmel verwendete Prozess-CPU-Zeit statt Wandzeit. Klein: vorher 0,549 s, danach 0,640 s; Stadion: vorher 2,450 s, danach 1,954 s. Die neue stabile Besetzung unterscheidet sich vom alten Zufallsablauf. Das ist ein Szenariovergleich, keine allgemeine Beschleunigungs- oder Studio-One-Leistungszusage.

Das Offline-Demowerkzeug aktiviert auf SSE-Plattformen dieselbe Denormal-Unterdrückung wie der Plugin-Callback. Sehr kleine Hallreste hatten den externen Renderer sonst deutlich verlangsamt.

## Expliziter Release-Auftrag per Commit

Neben dem manuellen Workflow kann ein ausdrücklich freigegebener Push auf `main` mit der ersten Commitzeile `release: Tumult <VERSION> <Beschreibung>` die Veröffentlichung beauftragen. Die Nummer muss exakt zur Datei `VERSION` passen. Normale Pushes veröffentlichen weiterhin nichts. Der Release-Auftrag durchläuft dieselben Versions-, Screenshot-, Build-, Regressionstest- und pluginval-Prüfungen wie der manuelle publish-Lauf. Diese Alternative ermöglicht eine vom Auftraggeber autorisierte Veröffentlichung über den GitHub-Connector ohne zusätzliche Browseranmeldung.

Die Abschlusskorrektur liefert für den diskreten Gemurmel-Schalter kanonische Hostwerte 0/1. Der Windows-Workflow wartet pluginval explizit ab, verlangt Exitcode 0 und die abschließende SUCCESS-Meldung und weist Fehlerzeilen zurück. Protokolle werden als Artefakt gesichert.
