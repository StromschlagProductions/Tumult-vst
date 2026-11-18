// Render real JUCE editor components; no image retouching. Args: output directory.
#include "PluginProcessor.h"
#include <iostream>

class Capture final : private juce::Timer
{
public:
    explicit Capture (juce::File destination) : output (std::move (destination))
    {
        output.createDirectory();
        processor.setCurrentProgram (2);   // Protestzug
        processor.prepareToPlay (48000, 512);
        // Eine kurze Sprechprobe durch die Engine schicken, damit die Anzeige die Menge mit leuchtenden Stimmen zeigt
        juce::AudioBuffer<float> b (2, 512);
        juce::MidiBuffer midi;
        for (int block = 0; block < 116; ++block)   // endet mitten in einem Ruf (1,24 s), damit die Stimmen leuchten
        {
            for (int i = 0; i < 512; ++i)
            {
                const float t = (float) (block * 512 + i) / 48000.0f;
                const float env = std::fmod (t, 0.5f) < 0.3f ? 1.0f : 0.0f;
                const float v = 0.25f * env * (std::sin (6.2832f * 140.0f * t) + 0.5f * std::sin (6.2832f * 280.0f * t) + 0.3f * std::sin (6.2832f * 560.0f * t));
                b.setSample (0, i, v); b.setSample (1, i, v);
            }
            processor.processBlock (b, midi);
        }
        startTimer (100);
    }
    int result = 0;
private:
    void timerCallback() override
    {
        if (index >= 2)
        {
            auto* manifest = new juce::DynamicObject();
            manifest->setProperty ("version", PLUGIN_VERSION_TEXT);
            manifest->setProperty ("images", images);
            const juce::var data (manifest);
            if (! output.getChildFile ("screenshots.json").replaceWithText (juce::JSON::toString (data)))
            { fail ("Could not write screenshot manifest"); return; }
            stopTimer(); juce::MessageManager::getInstance()->stopDispatchLoop(); return;
        }
        if (! editor)
        {
            juce::MemoryBlock state;
            processor.getStateInformation (state);
            auto xml = juce::AudioProcessor::getXmlFromBinary (state.getData(), (int) state.getSize());
            if (! xml) { fail ("Invalid processor state"); return; }
            xml->setAttribute ("theme", index);
            xml->setAttribute ("uiW", 680); xml->setAttribute ("uiH", 420);
            juce::AudioProcessor::copyXmlToBinary (*xml, state);
            processor.setStateInformation (state.getData(), (int) state.getSize());
            editor.reset (processor.createEditor());
            editor->setSize (680, 420); editor->setVisible (true);
            return; // allow attachments/timers to settle on the actual message loop
        }
        auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
        auto file = output.getChildFile (index == 0 ? "oberflaeche-dunkel.png" : "oberflaeche-hell.png");
        file.deleteFile();
        juce::FileOutputStream stream (file);
        if (! stream.openedOk() || ! juce::PNGImageFormat().writeImageToStream (image, stream))
        { fail ("Could not write screenshot"); return; }
        std::cout << file.getFullPathName() << std::endl;
        images.add (file.getFileName());
        editor.reset(); ++index;
    }
    void fail (const char* reason)
    {
        std::cerr << reason << std::endl; result = 1; stopTimer();
        juce::MessageManager::getInstance()->stopDispatchLoop();
    }
    TumultProcessor processor;
    juce::File output;
    std::unique_ptr<juce::AudioProcessorEditor> editor;
    juce::Array<juce::var> images;
    int index = 0;
};

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI initialise;
    if (argc != 2) { std::cerr << "Usage: Screenshot <output-directory>" << std::endl; return 1; }
    Capture capture (juce::File (juce::String::fromUTF8 (argv[1])));
    juce::MessageManager::getInstance()->runDispatchLoop();
    return capture.result;
}
