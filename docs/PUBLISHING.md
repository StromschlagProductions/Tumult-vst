# Veröffentlichung und Herstellerangaben

**Öffentlicher Hersteller:** Stromschlag Audio. **GitHub-Projekt:** StromschlagProductions/Tumult-vst.

Änderungen dürfen mit beliebigen Entwicklungswerkzeugen (Claude, Codex, ChatGPT, IDE) erstellt werden. Diese Werkzeuge sind keine Plugin-Hersteller. In JUCE/CMake, VST3-Metadaten, Benutzeroberfläche, README und Release-Titeln wird Stromschlag Audio beziehungsweise Stromschlag Productions als Marke verwendet. Historische Git-Commit-Autoren und Co-Autoren bleiben unverändert; fremde Lizenz- und Urheberrechtshinweise dürfen nicht entfernt werden.

## Release-Ablauf

1. Code prüfen und nach `main` übernehmen. Dabei läuft der Windows-Bau automatisch (Build, Regressionstests, pluginval Strictness 8).
2. Build, Regressionstests und pluginval Strictness 8 müssen erfolgreich sein.
3. Screenshots für die Version erzeugen (Workflow **Screenshots erzeugen**), falls sich Oberfläche oder Version geändert haben.
4. GitHub Actions → **Windows-VST3 bauen** → **Run workflow** auf `main` starten, Option **publish** aktivieren. Ohne ausdrückliche Freigabe entsteht kein öffentliches Release. Für die Version muss `docs/releases/<VERSION>.md` existieren.
5. Prüfen, ob `/releases/latest` auf ein veröffentlichtes stabiles Release mit Windows-VST3-ZIP zeigt.
6. Die Stromschlag-Homepage liest dieses Release automatisch (Plugin-Karte mit `data-release-repo="Tumult-vst"`). Bei GitHub-API-Problemen führt ihr Fallback zur aktuellen Release-Seite.

Ein Git-Push oder erfolgreicher Build ist **nicht** automatisch eine Freigabe. Keine automatische öffentliche Veröffentlichung bei jedem Commit.

Übergreifender Standard: https://github.com/StromschlagProductions/StromschlagProductions.github.io/blob/main/docs/RELEASE_STANDARD.md

Verbindliches öffentliches Versionsschema: `docs/VERSIONING.md` (ohne Build-Suffix).
