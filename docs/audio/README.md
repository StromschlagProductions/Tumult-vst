# Vergleichsbeispiele 1.3 / 1.4

Diese MP3-Dateien stammen aus der echten C++-Engine vor und nach der Änderung. Die Lautstärke wurde **nicht normalisiert**. Es wurde kein tatsächlicher Hörtest durchgeführt. Die Beispiele erlauben einen eigenen Vergleich, bestätigen aber keine subjektive Klangverbesserung.

| Beispiel | 1.3 | 1.4 |
|---|---|---|
| Raunen, 24 Sekunden | [vorher](raunen-1.3.mp3) | [nachher](raunen-1.4.mp3) |
| Rufstimmen, synthetische deutsche Sprache | [vorher](stimmen-1.3.mp3) | [nachher](stimmen-1.4.mp3) |

Raunen: eingebettete Originalaufnahmen, 48 kHz stereo, Seed 7, GEMURMEL AN, PEGEL 100 %, INTENSITÄT 0 %, MENGE 0 %, EIGENE STIMME aus, RAUM 0 %. Die Pause am Anfang von 1.4 gehört zur sporadischen Gestaltung. Mit Raum hallen die Einsätze nach.

Rufe: lokal mit eSpeak NG erzeugtes deutsches Sprachsignal („Wir wollen keine Mauer! Gebt den Weg frei! Wir sind hier und bleiben hier!“), 48 kHz stereo, Seed 7, Klein, MENGE 70 %, VIELFALT 100 %, CHAOS 45 %, RAUM 20 %, EIGENE STIMME aus, GEMURMEL aus. Dieses künstliche Testsignal ersetzt keine menschliche Aufnahme. Sprachsynthese wird ausschließlich für das Beispiel verwendet und ist kein Bestandteil des Plugins. Die Vergleichsdateien enthalten nur den Ausgang der Menge einschließlich Raum und drei Sekunden Nachlauf.

MP3 mit 128 kbit/s; Pegel- und Pausenmessungen wurden auf den unkomprimierten Float-Renderings vorgenommen. Reproduktion des Gemurmels verwendet die unveränderten Ogg-Dateien, extern nach 24-kHz-Mono-PCM dekodiert. Die Plugin-Prozessortests prüfen außerdem den echten JUCE-Decoderpfad.
