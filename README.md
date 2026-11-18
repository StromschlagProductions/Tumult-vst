# TUMULT – Audio Drama Crowd Simulator

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="docs/logo-dunkel.png">
  <img src="docs/logo-hell.png" alt="Stromschlag Audio" height="64">
</picture>

VST3-Effekt für Hörspiel-Produktion. TUMULT macht aus **einer einzigen** eingesprochenen Stimme eine Menschenmenge, die denselben
Text ruft, zum Beispiel einen Protest-Sprechchor. Du sprichst „Wir wollen keine Mauer!“ einmal ein, wählst Klein, Mittel, Groß
oder Stadion und hörst eine Menge, in der jeder ein bisschen früher oder später, höher oder tiefer, näher oder weiter weg ruft.

**⬇ Download (Windows · VST3 · Stereo):** https://github.com/StromschlagProductions/Tumult-vst/releases/latest

## Warum es dieses Plugin gibt

In Hörspielen kommen immer wieder Szenen vor, in denen mehr als eine Stimme gebraucht wird: eine Demo, ein Sprechchor, ein
Stadion, ein Saal, der mitruft. Ich habe meistens nur meine eigene Aufnahme, und zehn Mal einsprechen und übereinanderlegen
klingt nach zehn Mal derselben Person. Ein Crowd-Plugin eines anderen Herstellers erfüllt seinen Zweck, aber mir fehlte eine
Menge, die ich mit wenigen Griffen einstellen kann und bei der ich beim Hören nicht ins Zweifeln komme, ob da wirklich Leute
stehen.

TUMULT löst das mit einer Menge aus vielen einzelnen Stimmen, die ich auf der Spur mit meinem Ruf einsetze. Jede Stimme hat
einen eigenen Einsatz, eine eigene Tonlage und Klangfarbe und einen eigenen Platz im Raum. Es gibt vier Größen und nur wenige
Regler. Die Anzeige zeigt die Menge von oben, jeder Punkt ist eine Stimme.

TUMULT ist ein Werkzeug für meinen eigenen Alltag in der Hörspielproduktion, entstanden aus dem, was ich in solchen Szenen
immer wieder gebraucht habe.

*TUMULT soll niemandem die Erfahrung abnehmen, eine Menge selbst zu inszenieren. Es kann viel Spaß machen und lehrreich sein,
Sprechchöre mit mehreren Aufnahmen, mit Freunden und mit eigener Staffelung aufzubauen und zu hören, wie Versatz, Tonlage,
Abstand und Raum ineinandergreifen. Ich bin dankbar, dass ich diesen Weg selbst gegangen bin – auch wenn er manchmal umständlich
war. TUMULT soll das Gestalten nicht ersetzen, sondern die technische Umsetzung im Alltag einfacher machen.*

– Lars Eickstädt, Stromschlag Audio

## Oberfläche

| Dunkel (Standard) | Hell |
|---|---|
| ![Dunkle Oberfläche](docs/oberflaeche-dunkel.png) | ![Helle Oberfläche](docs/oberflaeche-hell.png) |

Der Umschalter oben rechts wechselt den Look, die Wahl wird mit dem Projekt gespeichert. Der **?**-Knopf
öffnet die Hilfe, über jedem Regler erscheint beim Darüberfahren ein kurzer Hinweis.

Die Oberfläche lässt sich am rechten unteren Fensterrand stufenlos von 50 % bis 300 % vergrößern oder
verkleinern. Das Seitenverhältnis bleibt dabei fest.

## So ist es gedacht

- TUMULT kommt **direkt auf die Spur mit der eingesprochenen Stimme** (Mono oder Stereo). Es ersetzt den Eingang durch die
  Menge. Deine eigene Stimme steht als Regler daneben (EIGENE STIMME) und lässt sich leise unter die Menge legen oder ganz
  abschalten.
- **GRÖSSE** wählt, wie viele Leute da sind und wo: Klein, Mittel, Groß, Stadion. Mit der Größe ändern sich die Zahl der
  Stimmen, die Streuung des Einsatzes, die Breite im Stereobild, die Dumpfheit der hinteren Reihen und der Raum.
- **Staffelung:** Wenige Stimmen stehen vorne und sind deutlich, hell und nah am Einsatz. Die Masse dahinter ist dumpfer,
  leiser und breiter gestreut. So entsteht Tiefe, statt dass alle gleich weit weg sind.
- **Jeder Ruf klingt anders:** In jeder Sprechpause (ab etwa einer halben Sekunde Stille) würfelt TUMULT die Menge neu: wer
  mitruft, wer früher oder später kommt, welche Stimmlage jeder hat. Mitten im Ruf ändert sich nie etwas, deshalb knackt
  nichts. **NEUE LEUTE** würfelt von Hand neu, ebenfalls in der nächsten Pause.
- **Fester Vorlauf von 200 ms:** TUMULT meldet ihn dem Host, der ihn ausgleicht. Dadurch darf ein Teil der Menge auch **vor** dem
  Einsatz deiner Stimme beginnen, so wie es in einer echten Menge ist, in der ein paar Leute früher anfangen.

## Größen

| GRÖSSE | Wann nehmen | Leute | Raum |
|---|---|---|---|
| Klein | Kneipe, Hinterhof, Treppenhaus | 10 | kurz und nah |
| Mittel | Marktplatz, Saal, Demo auf der Straße | 24 | mittlerer Raum |
| Groß | Halle, Sporthalle, große Versammlung | 40 | langer Raum, ein Echo von den Wänden |
| Stadion | Arena, Fußballstadion | 64 | langer Nachhall, zwei Echos von den Rängen |

Beim Umschalten blendet TUMULT weich über, die Größe lässt sich also auch mitten in der Szene automatisieren.

## Regler (alle automatisierbar)

**Oben**

| Regler | Wirkung |
|---|---|
| GRÖSSE | Klein, Mittel, Groß oder Stadion (siehe oben) |
| MENGE | Lautstärke der Menge. Sie ist so geeicht, dass die Menge bei etwa 70 % ungefähr so laut ist wie die eingesprochene Stimme, egal wie viele Leute es sind |
| Anzeige „DIE MENGE“ | Die Menge von oben: unten stehst du als Hörer, links/rechts ist das Stereobild, nach hinten wird es dumpfer und leiser. Jeder Punkt ist eine Stimme und leuchtet, wenn sie gerade ruft |
| NEUE LEUTE | würfelt die Menge in der nächsten Sprechpause neu |

**Unten, in drei Kästen**

| Kasten | Regler | Wirkung |
|---|---|---|
| **CHOR** | SYNCHRON | 0 % ein ungeordneter Haufen mit weit gestreutem Einsatz, 100 % fast im Gleichschritt wie ein eingeübter Sprechchor. Wie weit der Einsatz gestreut wird, hängt außerdem von der Größe ab |
| | VIELFALT | 0 % klingen alle ähnlich wie eine Gruppe, 100 % mischen sich tiefere und höhere Lagen (Männer, Frauen, Jugendliche, bis etwa fünf Halbtöne) mit eigenen Klangfärbungen |
| **RAUM** | RAUM | Wieviel Raum die Menge bekommt. Der Raum folgt der Größe. Er liegt auch auf der eigenen Stimme, damit sie in der Menge steht |
| **MIX** | EIGENE STIMME | dein eingesprochenes Original (mit 200 ms Vorlauf), ganz links aus |
| | OUTPUT | Gesamtpegel am Ausgang |

**Presets** (erscheinen in der Preset-Liste des Hosts, setzen nur die Regler): Kneipe, Kleine Demo, Protestzug, Marktplatz,
Großdemo auf dem Platz, Sporthalle, Stadion-Chor und Stadion-Jubel.

## Anwendung

**Protestzug:** Ruf einmal einsprechen („Wir wollen keine Mauer!“), TUMULT auf die Spur, Preset „Protestzug“. SYNCHRON etwas
höher, wenn die Leute geübt wirken sollen, tiefer für einen spontanen Haufen.

**Sprechchor im Stadion:** Preset „Stadion-Chor“, EIGENE STIMME leise lassen oder abschalten. Mit SYNCHRON steuerst du, wie
sauber der Chor im Takt ist.

**Saal, der mitruft:** Mittel oder Groß, VIELFALT hoch, SYNCHRON niedrig, ein bisschen mehr RAUM.

**Kneipe, in der ein paar Leute zustimmend rufen:** Klein, MENGE etwas zurück, EIGENE STIMME als Hauptstimme dazu.

**Mit echten Aufnahmen:** Eine echte Menge oder ein Atmo-Teppich darf leise darunter liegen. TUMULT liefert die Rufe auf den Punkt.

## Installation (Windows)

1. Über den Download-Link oben die ZIP-Datei `Tumult-v<Version>-Windows-VST3.zip` laden.
2. ZIP entpacken und den **ganzen Ordner** `Tumult.vst3` nach `C:\Program Files\Common Files\VST3` kopieren.
3. DAW neu starten bzw. Plugins neu scannen.

## Versionen

Die Versionsnummer steht in der Datei `VERSION`, im Plugin-Kopf und im Namen der Release. Sie wird bei jeder Änderung hochgezählt (1.0, 1.1, 1.2 …).

- **1.0** – Erste Version: der Stimmvervielfacher mit vier Größen, den Reglern MENGE, SYNCHRON, VIELFALT, RAUM, EIGENE STIMME und OUTPUT, einer Anzeige der Menge, NEUE LEUTE, acht Presets, Hell/Dunkel, skalierbarer Oberfläche und Hilfe. [Release-Hinweise](docs/releases/1.0.md).

Das Gemurmel im Hintergrund (Walla), das unter den Rufen eine aufgeregte Menge hörbar macht, folgt als zweiter Teil des Plugins.

## Aufbau

- `Source/TumultDSP.h`: Klang-Engine (reines C++ ohne JUCE, samplefrei)
- `Source/PluginProcessor.*`, `Source/PluginEditor.*`: JUCE-Plugin und Oberfläche
- `tools/render_demo.cpp`: rendert Hörbeispiele aus der echten Engine, mit Testsignal oder mit einer eigenen WAV-Aufnahme
- `.github/workflows/build.yml`: Windows-Bau mit Tests und Stabilitätstest (pluginval) bei jeder Änderung; Release nur von Hand mit Haken „publish“, siehe [docs/CI.md](docs/CI.md) und [docs/PUBLISHING.md](docs/PUBLISHING.md)

## Qualität und Prüfung

Wenige Regler heißen nicht, dass hier etwas schnell zusammengeschustert wurde. Die Einfachheit liegt in der Bedienung, nicht in der Technik dahinter. Jede Version durchläuft Prüfungen, wie sie auch bei kommerziellen Plugins üblich sind, und alles ist im Quellcode nachvollziehbar:

- **pluginval, Stufe 8:** Vor jeder Veröffentlichung läuft das Prüfprogramm pluginval von Tracktion auf strenger Stufe. Es lädt das Plugin wieder und wieder, wechselt Abtastraten und Puffergrößen, speichert und lädt Einstellungen und fährt alle Regler mit Zufallswerten durch.
- **Automatische Tests der Klangberechnung** laufen bei jeder Änderung. Sie prüfen unter anderem alle Abtastraten von 22,05 bis 192 kHz, alle vier Größen, ungültige Werte, die Streuung des Einsatzes, Stereo und Mono sowie den genauen Vorlauf. Außerdem laufen sie mit Speicher- und Fehlerprüfern (AddressSanitizer, UndefinedBehaviorSanitizer), die auch seltene Programmierfehler aufspüren.
- **Echtzeitsicher:** Während der Wiedergabe wird kein Speicher angefordert und nirgends gewartet. Das verhindert Aussetzer und Knackser, auch bei kleinen Puffern und vielen Instanzen. Das Ergebnis ist unabhängig davon, wie der Host den Audiostrom in Blöcke teilt (bit-genau getestet).
- **Mono-kompatibel:** Alle Stimmen sind zeitlich versetzt und im Stereobild verteilt. Die Mono-Summe löscht sich nicht aus und klingt nicht hohl.
- **Saubere Übergänge:** Alle Regler gleiten, die Größe blendet weich über. Auch bei laufend automatisierten Reglern knackt nichts, getestet mit einem reinen Ton.
- **Menge wird nur in Pausen neu gewürfelt:** Die Zusammensetzung ändert sich nie mitten im Ruf. Eine Instanz merkt sich ihre Menge im Projekt, dieselbe Aufnahme ergibt beim erneuten Abspielen dasselbe Ergebnis.
- **Fester, gemeldeter Vorlauf:** 200 ms, exakt getestet, vom Host ausgeglichen.
- **Kein Loop, keine Samples:** Die Menge entsteht ausschließlich aus deiner eigenen Aufnahme, es sind keine Audiodateien eingebettet.
- **Pegel nach Messung abgestimmt:** MENGE 70 % entspricht bei allen Größen ungefähr der Lautheit der Eingangsstimme. Die Hörbeispiele werden aus derselben Engine gerendert.
- **Rechenaufwand:** in meinen Messungen etwa 1 % eines Prozessorkerns für Klein und etwa 5 % für Stadion (48 kHz).

Ob es echt klingt, entscheidet am Ende das Ohr. Rückmeldungen zum Klang sind deshalb besonders willkommen.

---

## Hinweis / Disclaimer

**Open Source · Work in Progress**

Dieses Plugin wurde in erster Linie für meinen persönlichen Workflow bei der Hörspielproduktion entwickelt. Es ist auf meine eigenen Anforderungen zugeschnitten und muss daher nicht für jeden Anwendungsfall geeignet sein.

Das Projekt wird laufend weiterentwickelt. Trotz sorgfältiger Tests können Fehler oder Kompatibilitätsprobleme auftreten. Die Software wird ohne Gewährleistung bereitgestellt; die Nutzung erfolgt auf eigene Verantwortung. Eine Haftung für mögliche Schäden oder Datenverluste wird, soweit gesetzlich zulässig, ausgeschlossen. Es gelten die jeweiligen Lizenzbedingungen.

**Feedback, Fehlermeldungen und Verbesserungsvorschläge sind herzlich willkommen!**

Wenn dieses kleine Werkzeug auch anderen die Hörspielarbeit erleichtert und mehr Raum für Geschichten und menschliche Kreativität schafft, freut mich das ganz besonders.

Öffentliches Versionsschema und Release-Ablauf: [Versionsnummern](docs/VERSIONING.md).
