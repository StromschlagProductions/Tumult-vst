#include "PluginEditor.h"
#include <BinaryData.h>

namespace
{
juce::String u8 (const char* s) { return juce::String (juce::CharPointer_UTF8 (s)); }

const char* kSizeShort[tumult::kNumSizes] = { "Klein", "Mittel", "Gro\xc3\x9f", "Stadion" };
const char* kSizeNote[tumult::kNumSizes] = {
    "Kneipe, Hinterhof: 10 Leute, kurzer Raum",
    "Platz, Saal: 24 Leute, mittlerer Raum",
    "Halle: 40 Leute, Echo von den W\xc3\xa4nden",
    "Arena: 64 Stimmen, langer Nachhall, Echo von den R\xc3\xa4ngen" };
}

// Farben wie bei den anderen Stromschlag-Plugins (Schiefer-Gruen + Orange)
TumultEditor::Theme TumultEditor::darkTheme()
{
    return { juce::Colour (0xff182326), juce::Colour (0xff233539), juce::Colour (0xfff5781a),
             juce::Colour (0xffe8ecec), juce::Colour (0xff8fa1a5), juce::Colour (0xff35494e), true };
}
TumultEditor::Theme TumultEditor::lightTheme()
{
    return { juce::Colour (0xffd5dbda), juce::Colour (0xffe8eae9), juce::Colour (0xfff5781a),
             juce::Colour (0xff233539), juce::Colour (0xff5d6e72), juce::Colour (0xffb5bfbe), false };
}

// Inhalt der Onboard-Hilfe, in der Reihenfolge der Oberflaeche
static juce::AttributedString buildHelp (juce::Colour text, juce::Colour accent, juce::Colour dim)
{
    juce::AttributedString s;
    s.setWordWrap (juce::AttributedString::byWord);
    s.setJustification (juce::Justification::topLeft);

    auto head = [&] (const char* t)
    { s.append (u8 (t) + "\n", juce::Font (juce::FontOptions (14.0f, juce::Font::bold)), accent); };
    auto body = [&] (const char* t)
    { s.append (u8 (t) + "\n\n", juce::Font (juce::FontOptions (13.0f)), text); };
    auto note = [&] (const char* t)
    { s.append (u8 (t) + "\n\n", juce::Font (juce::FontOptions (12.0f)), dim); };

    s.append (u8 ("TUMULT \xe2\x80\x93 Hilfe") + u8 (" \xc2\xb7 v") + juce::String (PLUGIN_VERSION_TEXT) + "\n\n",
              juce::Font (juce::FontOptions (17.0f, juce::Font::bold)), accent);

    head ("Einrichten");
    body ("Du sprichst oder spielst einen Ruf einmal ein, zum Beispiel \xe2\x80\x9eWir wollen keine Mauer!\xe2\x80\x9c. TUMULT macht daraus eine Menschenmenge, die denselben Text ruft. Lege das Plugin direkt auf die Spur mit deiner Stimme (Mono oder Stereo). Die Menge ersetzt den Eingang; deine eigene Stimme steht als einzelner Regler daneben (EIGENE STIMME).");
    body ("TUMULT kann eine echte Gruppenaufnahme mit mehreren Sprecherinnen und Sprechern nicht vollständig ersetzen. Es hilft, wenn keine weiteren Stimmen zur Verfügung stehen, oder ergänzt vorhandene Gruppenaufnahmen, um eine Menge dichter und größer wirken zu lassen.");
    note ("TUMULT arbeitet mit einem festen Vorlauf von 500 Millisekunden, damit die Menge auch vor deinem Einsatz anfangen darf. Der Host gleicht das automatisch aus. Wer mit eingeschaltetem Monitoring live einspricht, nimmt dafür besser die trockene Spur.");

    head ("GR\xc3\x96SSE");
    body ("Wie viele Leute und wo sie stehen. Klein: 10 Leute, kurzer Raum (Kneipe, Hinterhof). Mittel: 24 Leute, mittlerer Raum (Platz, Saal). Gro\xc3\x9f: 40 Leute mit Echo von den W\xc3\xa4nden (Halle). Stadion: 64 Stimmen, langer Nachhall und Echo von den R\xc3\xa4ngen. Mit der Gr\xc3\xb6\xc3\x9f" "e ändern sich Zahl der Stimmen, Streuung, Stereobreite, Dumpfheit der hinteren Reihen und der Raum.");

    head ("MENGE");
    body ("Lautst\xc3\xa4rke der Menge. Sie ist so geeicht, dass die Menge bei etwa 70 % ungef\xc3\xa4hr so laut ist wie deine eingesprochene Stimme, egal wie viele Leute es sind.");

    head ("Die Anzeige");
    body ("Die Menge von oben: unten stehst du als H\xc3\xb6rer, links und rechts ist das Stereobild, nach hinten wird es dumpfer und leiser. Jeder Punkt ist eine Stimme und leuchtet, wenn sie gerade ruft. Vorne stehen wenige deutliche Stimmen, dahinter die Masse.");

    head ("NEUE LEUTE");
    body ("W\xc3\xbcrfelt die Besetzung neu: Wer steht wo und hat welche Stimmlage. Das geschieht in der n\xc3\xa4" "chsten Sprechpause. Die gew\xc3\xa4hlte Besetzung wird im Projekt gespeichert. Von allein w\xc3\xbcrfelt TUMULT nach jeder l\xc3\xa4ngeren Pause neu (je nach CHAOS und Gr\xc3\xb6\xc3\x9f" "e rund eine halbe bis eine Sekunde Stille). Dabei variieren Einsatz, Beteiligung und Ausdruck; Personen und Positionen bleiben erhalten.");

    head ("CHAOS");
    body ("Wie gleichm\xc3\xa4\xc3\x9fig die Menge ruft. Ganz links rufen die Leute fast im Gleichschritt, wie ein einge\xc3\xbc" "bter Sprechchor. Je weiter du aufdrehst, desto mehr streut der Einsatz: Manche rufen deutlich vor dir, andere erst eine Weile danach, und jeder hat sein eigenes Sprechtempo, einer etwas schneller, ein anderer etwas langsamer. Wie weit es streut, h\xc3\xa4ngt zus\xc3\xa4tzlich von der Gr\xc3\xb6\xc3\x9f" "e ab: in einem Stadion kommt der Ruf von weiter weg und ist weiter verschmiert.");

    head ("VIELFALT");
    body ("Wie unterschiedlich die Leute klingen. Ganz links klingen alle \xc3\xa4hnlich wie eine Gruppe. Weiter rechts mischen sich tiefere und h\xc3\xb6here Tonlagen (bis etwa sechs Halbt\xc3\xb6ne), dazu kommen eigene Klangf\xc3\xa4rbungen.");

    body ("Bei klar stimmhaften Rufen werden Tonlage und Klangfarbe der nahen Stimmen getrennt variiert. Bei Flüstern und rauen Passagen bleibt die bisherige Bearbeitung als Rückfall aktiv.");

    head ("GEMURMEL");
    body ("Ein Bett aus Gemurmel im Hintergrund, das unter den Rufen eine aufgeregte Menge h\xc3\xb6rbar macht. AN / AUS schaltet das Bett unabh\xc3\xa4ngig vom Eingang ein. PEGEL regelt die Lautst\xc3\xa4rke und bleibt beim Ausschalten erhalten. Auch auf einer leeren Spur ist kein Eingangssignal erforderlich. INTENSIT\xc3\x84T blendet stufenlos von leisem Raunen \xc3\xbc" "ber Gemurmel und angespannte Stimmung bis zur aufgebrachten Menge. Zuf\xc3\xa4llige Ausschnitte und weiche Wechsel reduzieren feste Wiederholungen; es l\xc3\xa4uft auch ohne Eingang und geht durch denselben Raum wie die Menge. Willst du nur das Gemurmel, stellst du MENGE auf null und schaltest EIGENE STIMME aus.");

    body ("Bei 0 % INTENSITÄT ist das Raunen leise und sporadisch, mit weichen Pausen. Bis 25 % geht es in ein durchgehendes Bett über. PEGEL bleibt der unabhängige Lautstärkeregler.");

    head ("RAUM");
    body ("Wieviel Raum die Menge bekommt. Der Raum folgt der GR\xc3\x96SSE: in der Kneipe kurz und nah, im Stadion mit langem Nachhall und einzelnen Echos von den R\xc3\xa4ngen. Er liegt auch auf deiner eigenen Stimme, damit sie in der Menge steht und nicht davor.");

    head ("EIGENE STIMME");
    body ("Dein eingesprochenes Original, in Stereo erhalten und mit dem festen Vorlauf. Ganz links ist es aus, dann ruft nur die Menge. Bei den Presets steht sie meist leise unter der Menge, damit man einen Ruf heraush\xc3\xb6rt, ohne dass er nach Einzelperson klingt.");

    head ("OUTPUT");
    body ("Gesamtpegel vor der Schutzs\xc3\xa4ttigung am Ausgang. Hohe Pegel k\xc3\xb6nnen den Klang verdichten.");

    head ("Presets");
    body ("Kneipe, Kleine Demo, Protestzug, Marktplatz, Gro\xc3\x9f" "demo auf dem Platz, Sporthalle, Stadion-Chor, Stadion-Jubel und Ausgangslage. Sie stellen nur die Regler ein und erscheinen in der Preset-Liste deines Hosts.");

    head ("Allgemein");
    body ("Doppelklick auf einen Drehregler oder Strg-Klick setzt ihn auf den Standardwert zur\xc3\xbck.");
    body ("Look: Dunkel / Hell umschalten, die Wahl wird mit dem Projekt gespeichert. Das Fenster ist am rechten unteren Rand skalierbar (50\xe2\x80\x93" "300 %).");
    note ("Alle Regler sind automatisierbar. Die Menge ist Teil des Projekts: Beim Speichern merkt sich TUMULT, aus welchen Leuten sie besteht. Bei gleichen Reglerwerten ist die Berechnung unabh\xc3\xa4ngig von der Puffergr\xc3\xb6\xc3\x9f" "e des Hosts. Die Menge ist mono-kompatibel: Alle Stimmen sind zeitlich versetzt und verteilt, die Mono-Summe l\xc3\xb6scht sich nicht aus.");

    head ("Idee und Konzept");
    body ("Lars Eickst\xc3\xa4" "dt, Stromschlag Audio");
    note ("Gebaut mit JUCE.");

    return s;
}

TumultEditor::TumultEditor (TumultProcessor& p) : AudioProcessorEditor (&p), processor (p), display (p.crowd())
{
    addAndMakeVisible (content);
    setResizable (true, true);
    setResizeLimits (kBaseW / 2, kBaseH / 2, kBaseW * 3, kBaseH * 3);   // 50 % bis 300 %
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) kBaseW / (double) kBaseH);

    logo = juce::ImageCache::getFromMemory (BinaryData::logo_png, BinaryData::logo_pngSize);
    theme = processor.uiTheme.load() == 1 ? lightTheme() : darkTheme();
    setLookAndFeel (&look);

    setupKnob (amount, "amount", "MENGE");
    setupKnob (chaos, "chaos", "CHAOS");
    setupKnob (variety, "variety", "VIELFALT");
    setupKnob (walla, "walla", "PEGEL");
    setupKnob (wallaInt, "wallaInt", u8 ("INTENSIT\xc3\x84T"));
    setupKnob (room, "room", "RAUM");
    setupKnob (own, "own", "EIGENE STIMME");
    setupKnob (output, "output", "OUTPUT");

    wallaStage.setJustificationType (juce::Justification::centred);
    wallaStage.setFont (juce::FontOptions (11.0f));
    content.addAndMakeVisible (wallaStage);
    wallaButton.setButtonText ("AUS");
    wallaButton.onStateChange = [this] { wallaButton.setButtonText (wallaButton.getToggleState() ? "AN" : "AUS"); };
    wallaButton.setClickingTogglesState (true);
    wallaButton.setTooltip (u8 ("Gemurmel einschalten: funktioniert auch ohne Aufnahme auf einer leeren Spur. PEGEL bleibt beim Ausschalten erhalten. Automatisierbar."));
    content.addAndMakeVisible (wallaButton);
    wallaAttachment = std::make_unique<APVTS::ButtonAttachment> (processor.apvts, "wallaEnabled", wallaButton);

    // GROESSE: 4-Wege-Wahlschalter
    sizeLabel.setText (u8 ("GR\xc3\x96SSE"), juce::dontSendNotification);
    content.addAndMakeVisible (sizeLabel);
    sizeNote.setJustificationType (juce::Justification::topLeft);
    content.addAndMakeVisible (sizeNote);
    for (int i = 0; i < tumult::kNumSizes; ++i)
    {
        auto& b = sizeButtons[i];
        b.setButtonText (u8 (kSizeShort[i]));
        b.setClickingTogglesState (true);
        b.setRadioGroupId (1);
        b.onClick = [this, i] { if (sizeAttachment) sizeAttachment->setValueAsCompleteGesture ((float) i); };
        content.addAndMakeVisible (b);
    }
    sizeAttachment = std::make_unique<juce::ParameterAttachment> (*processor.apvts.getParameter ("size"),
        [this] (float v) { updateSizeButtons (juce::roundToInt (v)); }, nullptr);
    sizeAttachment->sendInitialUpdate();

    // Anzeige und NEUE LEUTE
    crowdLabel.setText ("DIE MENGE", juce::dontSendNotification);
    content.addAndMakeVisible (crowdLabel);
    content.addAndMakeVisible (display);
    rerollButton.setButtonText ("Neue Leute");
    rerollButton.onClick = [this] { processor.rerollCrowd(); };
    content.addAndMakeVisible (rerollButton);

    themeButton.onClick = [this]
    {
        theme = theme.dark ? lightTheme() : darkTheme();
        processor.uiTheme.store (theme.dark ? 0 : 1);
        processor.updateHostDisplay (juce::AudioProcessorListener::ChangeDetails{}.withNonParameterStateChanged (true));
        applyTheme();
    };
    content.addAndMakeVisible (themeButton);

    helpButton.setButtonText ("?");
    helpButton.setTooltip (u8 ("Hilfe: was machen die Regler?"));
    helpButton.onClick = [this] { helpScroll.setVisible (! helpScroll.isVisible()); };
    content.addAndMakeVisible (helpButton);

    helpScroll.setViewedComponent (&helpText, false);
    helpScroll.setScrollBarsShown (true, false);
    helpScroll.setVisible (false);
    content.addChildComponent (helpScroll);   // startet ausgeblendet

    // Tooltips
    amount.slider.setTooltip (u8 ("Lautst\xc3\xa4rke der Menge. Bei etwa 70 % ist sie ungef\xc3\xa4hr so laut wie deine eingesprochene Stimme."));
    chaos.slider.setTooltip (u8 ("Wie ungeordnet die Menge ruft: links im Gleichschritt wie ein Sprechchor, rechts mit weit gestreutem Einsatz und unterschiedlichem Sprechtempo."));
    variety.slider.setTooltip (u8 ("Wie unterschiedlich die Leute klingen: links eine \xc3\xa4hnliche Gruppe, rechts unterschiedlichere Tonlagen und Klangfarben."));
    walla.slider.setTooltip (u8 ("Lautst\xc3\xa4rke des Gemurmels. Mit AN / AUS aktivieren, auch ohne Aufnahme. Das Gemurmel liegt unter den Rufen und geht durch denselben Raum. Es geht auch allein: MENGE auf null und EIGENE STIMME aus."));
    wallaInt.slider.setTooltip (u8 ("Wie aufgeregt das Gemurmel ist: links sporadisches, leises Raunen mit Pausen, in der Mitte angespannt, rechts eine aufgebrachte Menge. Die Stufen blenden stufenlos ineinander."));
    room.slider.setTooltip (u8 ("Wieviel Raum die Menge bekommt. Der Raum folgt der GR\xc3\x96SSE (Kneipe bis Stadion mit Echo von den R\xc3\xa4ngen)."));
    own.slider.setTooltip (u8 ("Dein eingesprochenes Original (mit 500 ms Vorlauf). Ganz links aus: dann ruft nur die Menge."));
    output.slider.setTooltip (u8 ("Gesamtpegel vor der Schutzs\xc3\xa4ttigung am Ausgang. Hohe Pegel k\xc3\xb6nnen den Klang verdichten."));
    rerollButton.setTooltip (u8 ("W\xc3\xbcrfelt die Menge in der n\xc3\xa4" "chsten Sprechpause neu: wer wo steht, wer fr\xc3\xbc" "her oder sp\xc3\xa4ter ruft, welche Stimmlage."));
    display.setTooltip (u8 ("Die Menge von oben. Unten bist du, links und rechts ist das Stereobild, hinten ist es dumpfer und leiser. Jeder Punkt ist eine Stimme."));
    for (int i = 0; i < tumult::kNumSizes; ++i)
        sizeButtons[i].setTooltip (u8 (kSizeNote[i]));

    applyTheme();
    updateMurmurStage();
    // zuletzt gespeicherte Fenstergroesse wiederherstellen (sonst 100 %)
    const int savedW = processor.uiWidth.load();
    const int restoredW = juce::jlimit (kBaseW / 2, kBaseW * 3, savedW);
    setSize (restoredW, juce::roundToInt ((float) restoredW * (float) kBaseH / (float) kBaseW));
    sizeReady = true;
    startTimerHz (30);
}

TumultEditor::~TumultEditor() { stopTimer(); setLookAndFeel (nullptr); }

void TumultEditor::updateSizeButtons (int index)
{
    index = juce::jlimit (0, tumult::kNumSizes - 1, index);
    for (int i = 0; i < tumult::kNumSizes; ++i)
        sizeButtons[i].setToggleState (i == index, juce::dontSendNotification);
    sizeNote.setText (u8 (kSizeNote[index]), juce::dontSendNotification);
}

void TumultEditor::updateMurmurStage()
{
    const char* stages[] = { "Raunen", "Gemurmel", "angespannt", "aufgebracht", "Tumult" };
    const int stage = juce::jlimit (0, 4, juce::roundToInt (processor.apvts.getRawParameterValue ("wallaInt")->load() * 4));
    wallaStage.setText (stages[stage], juce::dontSendNotification);
}

void TumultEditor::timerCallback()
{
    const bool wantsDark = processor.uiTheme.load() != 1;
    if (theme.dark != wantsDark) { theme = wantsDark ? darkTheme() : lightTheme(); applyTheme(); }
    const int width = processor.uiWidth.load();
    if (getWidth() != width) setSize (width, juce::roundToInt ((float) width * kBaseH / kBaseW));
    updateMurmurStage();
    display.repaint();
}

void TumultEditor::applyTheme()
{
    look.setColour (juce::Slider::thumbColourId, theme.accent);
    look.setColour (juce::Slider::rotarySliderFillColourId, theme.accent);
    look.setColour (juce::Slider::rotarySliderOutlineColourId, theme.line);
    look.setColour (juce::Slider::trackColourId, theme.accent.withAlpha (0.75f));
    look.setColour (juce::Slider::backgroundColourId, theme.line);
    look.setColour (juce::Slider::textBoxTextColourId, theme.text);
    look.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    look.setColour (juce::ComboBox::backgroundColourId, theme.bg);
    look.setColour (juce::ComboBox::textColourId, theme.text);
    look.setColour (juce::ComboBox::arrowColourId, theme.text);
    look.setColour (juce::ComboBox::outlineColourId, theme.line);
    look.setColour (juce::PopupMenu::backgroundColourId, theme.panel);
    look.setColour (juce::PopupMenu::textColourId, theme.text);
    look.setColour (juce::PopupMenu::highlightedBackgroundColourId, theme.accent);
    look.setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::black);
    look.setColour (juce::Label::textColourId, theme.text);
    look.setColour (juce::TextButton::buttonColourId, theme.dark ? theme.panel : theme.bg);
    look.setColour (juce::TextButton::buttonOnColourId, theme.accent);
    look.setColour (juce::TextButton::textColourOffId, theme.text);
    look.setColour (juce::TextButton::textColourOnId, juce::Colours::black);

    wallaStage.setColour (juce::Label::textColourId, theme.dim);
    sizeLabel.setColour (juce::Label::textColourId, theme.dim);
    crowdLabel.setColour (juce::Label::textColourId, theme.dim);
    sizeNote.setColour (juce::Label::textColourId, theme.dim);
    sizeNote.setFont (juce::FontOptions (11.0f));
    display.panel = theme.dark ? theme.bg : theme.bg.brighter (0.05f);
    display.line = theme.line;
    display.accent = theme.accent;
    display.dim = theme.dim;
    themeButton.setButtonText (theme.dark ? "Look: Dunkel" : "Look: Hell");
    helpText.setAttributed (buildHelp (theme.text, theme.accent, theme.dim), theme.panel);
    helpScroll.fill = theme.panel;
    helpScroll.edge = theme.line;
    sendLookAndFeelChange();
    repaint();
}

void TumultEditor::setupKnob (Knob& k, const juce::String& paramId, const juce::String& text)
{
    // nur senkrecht ziehen (wie bei den anderen Plugins): hoch = mehr, runter = weniger
    k.slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    k.slider.setMouseDragSensitivity (180);   // Pixel fuer den ganzen Regelweg
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
    content.addAndMakeVisible (k.slider);
    k.label.setText (text, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    content.addAndMakeVisible (k.label);
    k.attachment = std::make_unique<APVTS::SliderAttachment> (processor.apvts, paramId, k.slider);
}

void TumultEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme.bg);
    g.addTransform (juce::AffineTransform::scale (uiScale));
    const int W = kBaseW;

    // Kopfleiste: folgt dem Look (dunkel = dunkle Leiste + helles Logo, hell = helle Leiste + dunkles Logo)
    g.setColour (theme.panel);
    g.fillRect (0, 0, W, 64);
    g.setColour (theme.line);
    g.fillRect (0, 63, W, 1);
    g.setColour (theme.accent);
    g.fillRect (0, 0, 5, 64);

    if (logo.isValid())
    {
        g.setColour (theme.text);   // Logo ist eine Tinten-Maske und wird in der Textfarbe gezeichnet
        g.drawImage (logo, juce::Rectangle<float> (14.0f, 4.0f, 56.0f, 56.0f),
                     juce::RectanglePlacement::centred, true);
    }

    const float subX = 190.0f;
    g.setColour (theme.text);
    g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    g.drawText ("TUMULT", 80, 8, 108, 30, juce::Justification::centredLeft);
    g.setColour (theme.accent);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText ("AUDIO DRAMA CROWD SIMULATOR", (int) subX, 15, 300, 22, juce::Justification::centredLeft);
    g.setColour (theme.dim);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText (juce::String (juce::CharPointer_UTF8 ("STROMSCHLAG AUDIO  \xc2\xb7  v" PLUGIN_VERSION_TEXT)), 80, 36, 300, 16, juce::Justification::centredLeft);

    // Kleines Symbol: eine Gruppe von Koepfen (einfarbig, folgt dem Look)
    {
        juce::GlyphArrangement ga;
        ga.addLineOfText (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)),
                          "AUDIO DRAMA CROWD SIMULATOR", 0.0f, 0.0f);
        const float subEnd = subX + ga.getBoundingBox (0, -1, true).getWidth();   // Ende des Untertitels
        const float helpX  = (float) (kBaseW - 170);                              // linke Kante des Look-Knopfs
        const float mid    = 0.5f * (subEnd + helpX);                             // Mitte dazwischen
        g.setColour (theme.panel.interpolatedWith (theme.text, 0.30f));
        const float dots[][3] = { { -18, 8, 3.6f }, { -6, 4, 3.6f }, { 6, 8, 3.6f }, { 18, 4, 3.6f },
                                  { -12, 18, 3.2f }, { 0, 14, 3.2f }, { 12, 18, 3.2f }, { -18, 26, 2.8f }, { -6, 24, 2.8f },
                                  { 6, 26, 2.8f }, { 18, 24, 2.8f } };
        for (const auto& d : dots)
            g.fillEllipse (mid + d[0] - d[2], 17.0f + d[1] - d[2], d[2] * 2.0f, d[2] * 2.0f);
    }

    // Obere Karte: GROESSE | MENGE | ANZEIGE
    g.setColour (theme.panel);
    g.fillRoundedRectangle (12.0f, 76.0f, (float) W - 24.0f, 150.0f, 8.0f);
    g.setColour (theme.line);
    g.drawRoundedRectangle (12.5f, 76.5f, (float) W - 25.0f, 149.0f, 8.0f, 1.0f);
    g.fillRect (264.0f, 88.0f, 1.0f, 126.0f);
    g.fillRect (416.0f, 88.0f, 1.0f, 126.0f);

    // Untere Kaesten: CHOR | GEMURMEL | RAUM | MIX
    struct Box { float x, w; const char* title; const char* note; };
    const Box boxes[] = { { 12.0f, 220.0f, "CHOR", "wie die Menge ruft" },
                          { 240.0f, 220.0f, "GEMURMEL", "" },
                          { 468.0f, 124.0f, "RAUM", "" },
                          { 600.0f, (float) W - 612.0f, "MIX", "" } };
    for (const auto& b : boxes)
    {
        g.setColour (theme.panel);
        g.fillRoundedRectangle (b.x, 236.0f, b.w, 172.0f, 8.0f);
        g.setColour (theme.line);
        g.drawRoundedRectangle (b.x + 0.5f, 236.5f, b.w - 1.0f, 171.0f, 8.0f, 1.0f);
        g.setColour (theme.accent);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText (b.title, (int) b.x + 12, 242, (int) b.w - 24, 16, juce::Justification::centredLeft);
        g.setColour (theme.dim);
        g.setFont (juce::FontOptions (10.5f));
        g.drawText (u8 (b.note), (int) b.x + 12, 242, (int) b.w - 24, 16, juce::Justification::centredRight);
    }
}

void TumultEditor::resized()
{
    if (sizeReady)
    {
        processor.uiWidth.store (getWidth());
        processor.uiHeight.store (getHeight());
    }
    uiScale = std::min ((float) getWidth() / kBaseW, (float) getHeight() / kBaseH);
    content.setTransform (juce::AffineTransform::scale (uiScale));
    content.setBounds (0, 0, kBaseW, kBaseH);
    repaint();

    const int w = kBaseW;
    // Look links, Hilfe ganz rechts
    themeButton.setBounds (w - 170, 18, 112, 28);
    helpButton.setBounds (w - 48, 18, 32, 28);
    helpScroll.setBounds (12, 76, w - 24, 332);   // deckt Karte und Kaesten genau ab (y 76 bis 408)
    helpText.setWidth (helpScroll.getMaximumVisibleWidth());

    // Obere Karte
    sizeLabel.setBounds (24, 84, 200, 18);
    for (int i = 0; i < tumult::kNumSizes; ++i)
        sizeButtons[i].setBounds (24 + (i % 2) * 120, 108 + (i / 2) * 40, 112, 32);
    sizeNote.setBounds (24, 192, 232, 30);

    amount.label.setBounds (270, 84, 140, 18);
    amount.slider.setBounds (278, 104, 124, 118);

    crowdLabel.setBounds (432, 84, 224, 18);
    display.setBounds (432, 104, w - 456, 82);
    rerollButton.setBounds (432, 192, w - 456, 26);

    auto placeKnob = [] (Knob& k, int x, int y, int cw)
    {
        k.label.setBounds (x, y, cw, 18);
        k.slider.setBounds (x + 10, y + 20, cw - 20, 118);
    };
    // CHOR: CHAOS, VIELFALT
    placeKnob (chaos, 14, 264, 108);
    placeKnob (variety, 122, 264, 108);
    wallaButton.setBounds (354, 240, 96, 21);
    // GEMURMEL: PEGEL, INTENSITAET
    placeKnob (walla, 242, 264, 108);
    placeKnob (wallaInt, 350, 264, 108);
    wallaInt.slider.setBounds (360, 284, 88, 100);
    wallaStage.setBounds (350, 386, 108, 16);
    // RAUM
    placeKnob (room, 476, 264, 108);
    // MIX: EIGENE STIMME, OUTPUT
    placeKnob (own, 600, 264, 104);
    placeKnob (output, 704, 264, 104);
    helpScroll.toFront (false);
}
