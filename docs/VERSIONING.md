# Öffentliche Versionsnummern

`VERSION` enthält genau eine Nummer wie `1.0`, `1.1`, …, `1.9`, `2.0`, `2.1`, ….
Jede neue Veröffentlichung erhält die nächste Nummer; nach `.9` folgt die nächste
Vorkommastelle mit `.0`. Es gibt keine öffentliche Major-/Minor-/Patch-Unterscheidung
und keine Build-Suffixe. Interne Actions-Laufnummern sind nur interne Kennungen.

CMake liest `VERSION` für Plugin-Anzeige und VST3-Metadaten. Der Release-Workflow liest
dieselbe Datei für Tag, Titel und ZIP-Namen, prüft das Nummernschema und vergleicht die
gebauten VST3-/Windows-Binärmetadaten mit dieser Version. JUCE-interne Nullen wie
`1.6.0`/`1.6.0.0` bezeichnen dieselbe öffentliche Version `1.6`.

Releases erfordern einen ausdrücklich freigegebenen `build.yml`-Lauf auf `main`: manuell mit
`publish=true` oder über den unten beschriebenen Release-Commit. Er baut Windows-x64-VST3, führt Regressionstests und pluginval Stufe 8
aus und veröffentlicht erst danach die ZIP als neuestes stabiles Release. Die passenden
Hinweise stehen in `docs/releases/<VERSION>.md`. Alte Releases werden nicht entfernt.

## Expliziter Release-Auftrag per Commit

Neben dem manuellen Workflow kann ein ausdrücklich freigegebener Push auf `main` mit der ersten Commitzeile `release: Tumult <VERSION> <Beschreibung>` die Veröffentlichung beauftragen. Die Nummer muss exakt zur Datei `VERSION` passen. Normale Pushes veröffentlichen weiterhin nichts. Der Release-Auftrag durchläuft dieselben Versions-, Screenshot-, Build-, Regressionstest- und pluginval-Prüfungen wie der manuelle publish-Lauf. Diese Alternative ermöglicht eine vom Auftraggeber autorisierte Veröffentlichung über den GitHub-Connector ohne zusätzliche Browseranmeldung.
