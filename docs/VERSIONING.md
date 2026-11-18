# Öffentliche Versionsnummern

`VERSION` enthält genau eine Nummer wie `1.0`, `1.1`, …, `1.9`, `2.0`, `2.1`, ….
Jede neue Veröffentlichung erhält die nächste Nummer; nach `.9` folgt die nächste
Vorkommastelle mit `.0`. Es gibt keine öffentliche Major-/Minor-/Patch-Unterscheidung
und keine Build-Suffixe. Interne Actions-Laufnummern sind nur interne Kennungen.

CMake liest `VERSION` für Plugin-Anzeige und VST3-Metadaten. Der Release-Workflow liest
dieselbe Datei für Tag, Titel und ZIP-Namen, prüft das Nummernschema und vergleicht die
gebauten VST3-/Windows-Binärmetadaten mit dieser Version. JUCE-interne Nullen wie
`1.6.0`/`1.6.0.0` bezeichnen dieselbe öffentliche Version `1.6`.

Releases erfordern den manuell freigegebenen `build.yml`-Lauf auf `main` mit
`publish=true`. Er baut Windows-x64-VST3, führt Regressionstests und pluginval Stufe 8
aus und veröffentlicht erst danach die ZIP als neuestes stabiles Release. Die passenden
Hinweise stehen in `docs/releases/<VERSION>.md`. Alte Releases werden nicht entfernt.
