#pragma once
#include "PluginProcessor.h"

// Onboard-Hilfe: formatierter Text (Ueberschriften fett in Akzentfarbe), scrollbar in einem Viewport
class HelpView : public juce::Component
{
public:
    void setAttributed (const juce::AttributedString& s, juce::Colour background)
    {
        text = s; bg = background; relayout();
    }
    void setWidth (int w)
    {
        if (w != laidOutW) { laidOutW = w; relayout(); }
    }
    void paint (juce::Graphics& g) override
    {
        // transparent: der abgerundete Hintergrund kommt vom HelpPanel
        tl.draw (g, getLocalBounds().toFloat().reduced (14.0f, 10.0f));
    }
private:
    void relayout()
    {
        const int w = juce::jmax (120, laidOutW);
        tl.createLayout (text, (float) (w - 28));
        setSize (w, (int) std::ceil (tl.getHeight()) + 24);
        repaint();
    }
    juce::AttributedString text;
    juce::TextLayout tl;
    juce::Colour bg;
    int laidOutW = 0;
};

// Hilfe-Flaeche: Viewport mit abgerundetem Hintergrund (passt auf die Kartenflaeche darunter)
class HelpPanel : public juce::Viewport
{
public:
    juce::Colour fill, edge;
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (fill);
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (edge);
        g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
    }
};

// Die Menge von oben gesehen: unten steht der Hoerer, links/rechts ist das Stereobild, nach hinten wird es dumpfer.
// Jeder Punkt ist eine Stimme und leuchtet, wenn sie gerade ruft.
class CrowdDisplay : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit CrowdDisplay (const tumult::Engine& e) : engine (e) {}
    juce::Colour panel, line, accent, dim;
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (panel);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (line);
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);

        const float pad = 10.0f;
        const auto area = r.reduced (pad, pad);
        // Die Hoererposition: kleines Dreieck unten in der Mitte
        juce::Path me;
        me.addTriangle (r.getCentreX() - 5.0f, r.getBottom() - 5.0f, r.getCentreX() + 5.0f, r.getBottom() - 5.0f,
                        r.getCentreX(), r.getBottom() - 12.0f);
        g.setColour (dim);
        g.fillPath (me);

        for (int i = 0; i < tumult::kMaxVoices; ++i)
        {
            if (engine.voiceOn (i) < 0.5f) continue;
            const float pan = engine.voicePan (i), depth = engine.voiceDepth (i), level = std::sqrt (engine.voiceLevel (i));
            const float x = area.getCentreX() + pan * 0.5f * area.getWidth();
            const float y = area.getBottom() - 8.0f - depth * (area.getHeight() - 14.0f);
            const float rad = 2.0f + 1.0f * (1.0f - depth) + 2.4f * level;
            g.setColour (accent.withAlpha (0.20f + 0.80f * level));
            g.fillEllipse (x - rad, y - rad, rad * 2.0f, rad * 2.0f);
        }
    }
private:
    const tumult::Engine& engine;
};

class TumultEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit TumultEditor (TumultProcessor&);
    ~TumultEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;
    struct Knob
    {
        juce::Slider slider; juce::Label label;
        std::unique_ptr<APVTS::SliderAttachment> attachment;
    };
    struct Theme { juce::Colour bg, panel, accent, text, dim, line; bool dark; };

    void setupKnob (Knob&, const juce::String& paramId, const juce::String& text);
    void applyTheme();
    void updateSizeButtons (int index);
    void timerCallback() override;
    static Theme darkTheme();
    static Theme lightTheme();

    static constexpr int kBaseW = 680, kBaseH = 420;

    TumultProcessor& processor;
    juce::LookAndFeel_V4 look;
    juce::Component content;      // feste 680x420-Flaeche, wird skaliert
    float uiScale = 1.0f;
    Theme theme;
    bool sizeReady = false;   // erst nach dem Wiederherstellen der Groesse Fenstergroesse speichern
    juce::Image logo;

    Knob amount, tight, variety, room, own, output;
    juce::TextButton sizeButtons[tumult::kNumSizes];
    juce::Label sizeLabel, sizeNote, crowdLabel;
    std::unique_ptr<juce::ParameterAttachment> sizeAttachment;
    CrowdDisplay display;
    juce::TextButton rerollButton;
    juce::TextButton themeButton;
    juce::TextButton helpButton;
    HelpPanel helpScroll;
    HelpView helpText;
    juce::TooltipWindow tooltipWindow { this, 400 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TumultEditor)
};
