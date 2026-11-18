# Validierung TUMULT 1.4

## Stand und Grenzen

Diese Version führt eine bedingte periodensynchrone Vordergrundbearbeitung ein und verändert das Raunen bei 0 % bewusst hörbar. Parameterkennungen, Presetplätze, Plugin-Kennung und gemeldete 500 ms Latenz bleiben erhalten.

Es wurden keine tatsächlichen Hörtests und keine Studio-One-Pro-8-Sitzung durchgeführt. Die Messungen belegen bestimmte technische Eigenschaften, keine bestätigte Natürlichkeit. [Unnormalisierte A/B-Beispiele](audio/README.md) stehen zur eigenen Beurteilung bereit. Das Sprachbeispiel verwendet ausdrücklich synthetische Sprache.

## Abgeschlossene lokale Prüfungen

| Prüfung | Ergebnis |
|---|---|
| Vollständiger Linux-JUCE-/VST3-Release-Build | Erfolgreich |
| Native DSP-, Stimmkomponenten- und Prozessortests | 3/3 bestanden, 31,07 s |
| Separater portabler Release-Build | 2/2 bestanden, 63,90 s bei gleichzeitiger Build-Auslastung |
| Lokaler ASan-/UBSan-Build | 2/2 bestanden, 207,41 s bei gleichzeitiger Build-Auslastung |
| Linux-pluginval, Strictness 8, eigener Prozess mit Xvfb | Exitcode 0, abschließendes `SUCCESS`, keine Fehlerzeilen |
| Echte Screenshots Dunkel/Hell | Aus Version 1.4 erzeugt und visuell geprüft |
| Windows-x64-VST3, Versions- und Herstellerprüfung | Erfolgreich |
| Windows-DSP-, Stimmkomponenten- und Prozessortests | 3/3 bestanden |
| Windows-pluginval, Strictness 8 | Vollständig beendet, Exitcode 0, abschließendes `SUCCESS`, keine Fehlerzeilen |
| GitHub-Linux-CI, ASan/UBSan | 2/2 bestanden, 71,28 s |

Produktionscode: `30075e278d96b668e0c3449ab8137dce52b0d6be`. Spätere Ergänzungen betreffen Dokumentation, Audiodateien und Screenshots.

Nachweise: [Windows-Build und pluginval](https://github.com/StromschlagProductions/Tumult-vst/actions/runs/44387872555); [Linux-Lauf](https://github.com/StromschlagProductions/Tumult-vst/actions/runs/44387872380).

## Gezielte Prüfungen

- Bekannter synthetischer Quell-/Filter-Vokal mit 120 Hz Grundton: Tonhöhenfaktor 1,2 ergibt etwa 144 Hz, auch bei Formantfaktoren 0,9 / 1,0 / 1,1 und verändertem Quellzeitverlauf. Geprüft bei 44,1 und 48 kHz, Toleranz 1,5 Hz. Formantfaktoren verändern das Verhältnis gemessener Obertonleistungen bei gleicher Grundtonhöhe.
- Bitgleiche Komponentenverarbeitung über den Überlauf des 32-Bit-Samplezählers; keine Zugriffe auf zukünftige Samples, keine normalen C++-Heap-Allokationen, endliche begrenzte Ausgänge.
- Unperiodisches Rauschen wird nicht zuverlässig stimmhaft erzwungen; ungültige Eingangswerte bleiben numerisch beherrscht. Ohne stimmhafte Analyse bleibt der Rückfallpfad unverändert.
- Sporadisches Raunen gegenüber durchgehendem Bett: geringerer RMS-Pegel, nachweisbare ruhige Fenster bei 0 %, keine solchen Fenster bei 25 %, begrenzte Schrittweite der Ereignishüllkurve.
- Geplante Ausschnitte enden vor dem Aufnahmeende; die ersten drei Übergänge verwenden unterschiedliche Takes. Bestehende Prüfungen auf Determinismus, Blockteilung, Parameterautomation, Sample-Rates und C++-Allokationen bleiben aktiv.
- NEUE LEUTE meldet eine speicherrelevante Nichtparameter-Zustandsänderung an den Host.
- Die vorhandenen Prozessorprüfungen prüfen weiterhin den echten Decoderpfad der eingebetteten Aufnahmen, unabhängigen Generatorbetrieb, Mono-/Stereo-Busse, Parametertexte, Bypass, Recall und Editor.

Die Allokations-Hooks überwachen normale C++-Allokationen, keine vollständige Betriebssystem-Instrumentierung.

## Raunen: Messung mit den echten Aufnahmen

60 Sekunden je Stellung, Seed 7, 48 kHz stereo, kein Eingang, MENGE 0 %, EIGENE STIMME aus, GEMURMEL AN, PEGEL 100 %, RAUM 0 %. Erste Sekunde für die Statistik verworfen. Ruhige Fenster: beide Kanäle in einem 100-ms-Fenster mit RMS unter 10⁻⁶ (−120 dBFS). RMS ist der gemeinsame lineare Kanal-RMS, keine LUFS-Messung.

| Intensität | RMS 1.3 | RMS 1.4 | Ruhige Fenster 1.3 / 1.4 |
|---|---:|---:|---:|
| 0 % | −23,97 dBFS | −34,21 dBFS | 0 % / 41,7 % |
| 12,5 % | −23,63 dBFS | −28,93 dBFS | 0 % / 0 % |
| 25 % | −23,17 dBFS | −23,35 dBFS | 0 % / 0 % |
| 100 % | −20,81 dBFS | −20,52 dBFS | 0 % / 0 % |

Bei 0 % spielen je Kanal weiche Gruppen von ungefähr 0,9–2,4 Sekunden, getrennt durch ungefähr 1,8–4,8 Sekunden Pause. Durch die unabhängigen Kanäle ist nicht jede Kanalpause eine vollständige Stereopause. Ein sanfter Dichteverlauf ergänzt bis 25 % wieder das durchgehende Bett. Die zusätzliche Pegelabsenkung an der niedrigsten Stellung beträgt 3 dB; die übrige RMS-Absenkung entsteht vor allem durch Pausen und Hüllkurven. Raumfahnen können diese Pausen überbrücken.

Die Abschnittsplanung nutzt zeitlich begrenzte Ausschnitte, gemischte Take-Reihenfolgen und eine begrenzte Startpunkthistorie. Sie reduziert systematische Wiederholungen und denselben Endabschnitt. Sie trennt keine einzelnen Sprecher aus den vorhandenen Gruppenaufnahmen. Alle 14 Dateien bleiben unverändert; die Stufe mit nur einer Aufnahme kann keine neue Materialvielfalt erhalten.

Die synthetischen Rufbeispiele bleiben numerisch endlich und begrenzt. Mit identischer Einstellung lag der Ausgangs-RMS vor/nach der Änderung bei −27,22 / −27,42 dBFS; Spitzen bei −5,76 / −5,06 dBFS. Dies sind keine Messungen der Verständlichkeit.

## Vordergrundmodell

Eine gemeinsame kausale Periodizitätsanalyse arbeitet mit ungefähr 12 kHz intern und einem neuen Analysefenster alle 5 ms. Die bereits vorhandene Plugin-Verzögerung ermöglicht den Zugriff auf analysierte Quellpassagen. Für die je nach Größe ersten zwei bis sechs Stimmen werden bei hoher Analysekonfidenz Hann-Fenster an geschätzten Periodenmarken überlagert. Der Syntheseabstand steuert die Tonhöhe, die Abspielrate im Fenster separat die spektrale Hülle. VIELFALT verteilt beide Eigenschaften unabhängig je Person; es gibt keinen zusätzlichen Bedienparameter.

Die Quellposition folgt weiterhin dem vorhandenen begrenzten Zeitverlauf. Es ist kein frei steuerbarer, vollwertiger WSOLA- oder Phonemgenerator. Bei unsicherer Periodizität, Flüstern oder rauen Passagen erfolgt ein ungefähr 25-ms-Übergang zurück zum bisherigen Delay-Pfad. Hintere Stimmen verwenden weiterhin diesen Pfad. Verschiedene Alter, Geschlechter und unabhängig gesprochene Phoneme werden nicht garantiert. Der neue Pfad benötigt zusätzliche CPU-Zeit.

Die Implementierung ist eigenständiger C++-Code ohne neue Plugin-Abhängigkeit. Methodische Grundlagen: [de Cheveigné / Kawahara, YIN (2002)](https://www.ee.columbia.edu/~dpwe/e6820/papers/deChevK02-yin.pdf), [Moulines / Charpentier, PSOLA (1990)](https://courses.grainger.illinois.edu/ECE420/fa2022/5_PSOLA.pdf) und [Praat: Overlap-add](https://www.fon.hum.uva.nl/praat/manual/overlap-add.html). Diese Referenzen sind keine Bestätigung dieser Implementierung durch ihre Autoren.

## Rechenaufwand

Ein separater GCC-14.2-`-O2`-Benchmark auf dem Xeon Platinum 8573C verwendete 48 kHz, 512 Samples, Seed 7, zwölf Sekunden Dauerton, Raum 35 %, keine Eigenstimme und kein Gemurmel. Gemessen wurde Prozess-CPU-Zeit: Klein 1.3 / 1.4: 0,214 / 0,417 s; Stadion: 0,951 / 1,416 s. Das entspricht in diesem einzelnen Lauf ungefähr 1,8 / 3,5 % beziehungsweise 7,9 / 11,8 % einer CPU-Kernzeit. Frühere Messungen während paralleler Builds schwankten deutlich. Das neue Verfahren kostet zusätzliche Rechenzeit; daraus folgt keine allgemeine Leistungszusage und keine Messung auf dem Studio-One-Rechner.

## Reproduktion

```sh
cmake -S . -B build/dsp -DAUDIO_DSP_ONLY=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build/dsp --parallel 2
ctest --test-dir build/dsp --output-on-failure
```

Der Windows-Workflow ergänzt JUCE-Prozessortests und einen vollständig abgewarteten pluginval-Lauf mit Strictness 8. Der Linux-Workflow prüft die beiden portablen Tests mit ASan und UBSan. Die oben verlinkten Läufe prüfen den unveränderten Produktionscode. Der Release auf main durchläuft dieselben Versions-, Screenshot-, Regressionstest- und pluginval-Prüfungen erneut.
