# Validierung TUMULT 1.3

## Ergebnis und Grenzen

Der endgültige Produktionscode wurde auf dem Arbeitszweig mit Commit `60f4518af202de913ed177fb077744bcc87f47b2` geprüft. Die abschließenden Ergänzungen betreffen Screenshots und Dokumentation.

Es wurden keine tatsächlichen Hörtests und keine Studio-One-Pro-8-Sitzung durchgeführt. Natürlichkeit und Verständlichkeit in einer echten Hörspielmischung sind deshalb keine bestätigten Hörergebnisse. Der unabhängige Gemurmelbetrieb wurde mit stillen und deaktivierten Eingängen im echten JUCE-Prozessor getestet; Stop, Suspend und Export müssen in Studio One gesondert beurteilt werden.

## Tatsächlich abgeschlossene Prüfungen

| Prüfung | Ergebnis |
|---|---|
| Windows-x64-VST3-Build, Versions- und Herstellerprüfung | Erfolgreich |
| Windows-DSP-Regressionen | Erfolgreich, 23,98 s |
| Windows-Prozessorregressionen einschließlich Editor | Erfolgreich, 1,72 s |
| Windows-pluginval, Strictness 8 | Vollständig beendet, Exitcode 0, abschließendes `SUCCESS`, keine Fehlerzeilen |
| Linux-CI mit AddressSanitizer und UndefinedBehaviorSanitizer | Erfolgreich |
| Lokaler vollständiger JUCE-/VST3-Build unter Linux | Erfolgreich |
| Lokale Prozessorregressionen einschließlich fraktionaler Schalterwerte | Erfolgreich, 6,12 s |
| Lokales pluginval unter Linux, Strictness 8, mit Xvfb | Vollständig beendet, Exitcode 0, abschließendes `SUCCESS` |
| Lokaler Release-DSP-Build nach der Bypass-Ergänzung | Erfolgreich, 175,85 s in parallel ausgelasteter Cloud-Umgebung |
| Echte JUCE-Oberflächenbilder, Dunkel und Hell | Neu erzeugt, Version 1.3, visuell geprüft; keine Bildretusche |

Nachweise: [vollständiger Windows-Lauf](https://github.com/StromschlagProductions/Tumult-vst/actions/runs/44375981162), [ASan/UBSan-Lauf](https://github.com/StromschlagProductions/Tumult-vst/actions/runs/44375981344).

pluginval prüft unter anderem Zustandswiederherstellung, Automation einschließlich Editor, Parameter-Threadsicherheit, Buskonfigurationen und zufällige Parameterwerte. Ein bestandener Lauf bestätigt keine subjektive Klangqualität und ersetzt keine DAW-Sitzung.

## Gezielte Regressionen

- Prozenttexte, Dezimalkomma, „aus“ und Intensitätsnamen.
- Diskreter Gemurmel-Schalter bei fraktionalen Hostwerten, insbesondere 0,469368: kanonische Werte 0/1 vor und nach Recall.
- Mono-Begrenzung bei maximalen gültigen Eigenstimmen-/Outputwerten, 44,1 und 48 kHz.
- Stereo-Eigenstimme und exakte gemeldete Verzögerung, auch im Host-Bypass.
- Generatorbetrieb mit deaktiviertem Eingang, AN/AUS, erhaltener Pegel und Tail-Meldung.
- Recall vor und nach Vorbereitung, gespeicherte manuelle Besetzung und Migration alter Zustände.
- Stabile Positionen über automatische Phrasenwechsel.
- Nach zehn Sekunden Stille kein verbrauchter Tempo-Versatz; während Sprache entsteht ein begrenzter Versatz.
- Bestehende Prüfungen für 22,05 bis 192 kHz, feste Parameter bei verschiedener Blockteilung, Automation, ungültige Werte, numerische Grenzen und C++-Allokationen im Callback.

Die Allokationsprüfungen überwachen normale C++-Allokationen über die vorhandenen Test-Hooks. Sie sind keine vollständige Instrumentierung sämtlicher Betriebssystem- und Bibliotheksoperationen.

## Messungen und Vergleichsrenderings

Bei 44,1 und 48 kHz ergaben externe Messprogramme nach zehn Sekunden Stille 0 ms Tempo-Akkumulation. Nach einer Sekunde Sprache lag der maximale individuelle Versatz bei ungefähr 36,5 ms. Der Mono-Spitzenwert bei maximalen gültigen Verstärkungen blieb bei höchstens 1,0 (0 dBFS).

Alle 14 echten Gemurmeldateien wurden untersucht; neun feste Intensitätsstellungen wurden jeweils 60 Sekunden gerendert. Die langfristigen Pegel bei festen Stufen bleiben gegenüber 1.2 praktisch unverändert. Die untersuchten Abschnitte hatten kleine DC-Anteile und geringe Links-Rechts-Korrelation. Das bestätigt weder die wahrgenommene Natürlichkeit noch die Unhörbarkeit wiederkehrender Einzelereignisse.

Das vorhandene Demo-Werkzeug wurde mit seinem synthetischen Silbensignal ausgeführt. Ausgangs-RMS für Klein/Mittel/Groß/Stadion: vorher −30,9/−30,3/−30,6/−30,7 dBFS, danach −30,7/−30,8/−30,9/−30,7 dBFS. Eingangs-RMS jeweils −32,2 dBFS. Eigenstimme und Raum sind enthalten; dies ist keine isolierte LUFS-Messung echter Sprache.

Ein separater Engine-Benchmark verwendete GCC 14.2, `-O2`, Xeon Platinum 8573C, 48 kHz, 512 Samples, zwölf Sekunden Dauerton, Raum 35 %, keine Eigenstimme und kein Gemurmel. Gemessen wurde Prozess-CPU-Zeit statt Wandzeit: Klein vorher 0,549 s, danach 0,640 s; Stadion vorher 2,450 s, danach 1,954 s. Die neue Besetzung unterscheidet sich vom alten Zufallsablauf. Das ist ein Szenariovergleich, keine allgemeine Beschleunigungs- oder Studio-One-Leistungszusage.

Das Offline-Demowerkzeug aktiviert auf SSE-Plattformen wie der Plugin-Callback die Denormal-Unterdrückung. Sehr kleine Hallreste hatten den externen Renderer sonst deutlich verlangsamt.

## Bewusste technische Entscheidungen

Personenmerkmale bleiben pro Besetzung stabil. Pro Phrase variieren Beteiligung, Einsatz und Pegelausdruck. NEUE LEUTE wählt eine neue gespeicherte Besetzung in einer ausreichend langen Eingangspause. Tempo-Versatz wächst während tatsächlich gelesener Sprache, nicht schon während Stille. Kleine Transpositionen werden nicht mehr pauschal unter einem halben Halbton auf Unisono gesetzt.

Zwei vorreservierte Räume überblenden Größenwechsel über etwa 500 ms. Dabei läuft die alte Hallstruktur weiter; kurzzeitig steigen Speicherbedarf und Rechenaufwand. Filtergrößenwechsel werden ebenfalls überblendet. Der Stereoanteil der Eigenstimme bleibt in einem eigenen Verzögerungspuffer erhalten.

Mono verwendet den Kanal-Mittelwert vor der Begrenzung. Gegenüber 1.2 kann das etwa 3 dB weniger Pegel bedeuten und verhindert den bisherigen Summenaufschlag. Die aktive Ausgangsstufe ist eine Schutzsättigung, kein True-Peak-Limiter. Der Bypass reicht das verzögerte Original mit weichem Übergang durch.

Die Pitch-Engine bleibt ein Delay-Verfahren. Eine unabhängige Formant-, WSOLA- oder neuronale Engine wurde ohne belastbare Sprach-Hörvergleiche nicht zum neuen Standard erklärt. Alle Rufstimmen übernehmen weiterhin dieselbe Darbietung; Alter, Geschlecht und unabhängig gesprochene Phoneme werden nicht garantiert modelliert.

Das Projekt speichert Parameter und Besetzung, keinen vollständigen Snapshot von Hallpuffern, Phrasenpositionen oder laufenden Gemurmel-Abspielköpfen. Ein Lauf aus gleichem Startzustand ist reproduzierbar; beliebige Transportverläufe müssen nicht identisch klingen.

Das Gemurmel besitzt 14 unveränderte 30-Sekunden-Aufnahmen (4/4/1/3/2). Zufällige Ausschnitte und weiche Wechsel reduzieren feste Loops; wiedererkennbare Ereignisse bleiben möglich. Es wurde kein zusätzliches Audiomaterial eingebaut.

## Korrigierte CI-Auswertung

Der bisherige Windows-Workflow wartete das als GUI-Anwendung gestartete pluginval nicht zuverlässig vollständig ab. Eine grüne Workflow-Anzeige allein war deshalb kein ausreichender Validierungsnachweis. So wurde auch der Recall-Fehler des neuen Bool-Schalters erst im vollständigen Protokoll sichtbar.

Der korrigierte Workflow wartet den Prozess explizit ab, verlangt Exitcode 0 und die abschließende SUCCESS-Meldung und weist Fehlerzeilen zurück. Die Protokolle werden als Artefakt gesichert. Der Schalter liefert nach außen passend zum diskreten Projektzustand kanonische Werte 0/1.

## Reproduktion und Veröffentlichung

```sh
cmake -S . -B build/dsp -DAUDIO_DSP_ONLY=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build/dsp --parallel 2
ctest --test-dir build/dsp --output-on-failure
```

Der vollständige Windows-Workflow baut und testet VST3 und führt pluginval Strictness 8 aus. Ein Release erfordert weiterhin eine ausdrückliche Freigabe: manueller `publish`-Haken oder ein versionsgebundener Commit `release: Tumult <VERSION> <Beschreibung>` auf `main`. Normale Pushes veröffentlichen nichts. Für beide Freigabewege gelten dieselben Build-, Versions-, Screenshot-, Regressionstest- und pluginval-Prüfungen.
