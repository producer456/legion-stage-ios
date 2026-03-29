#include <JuceHeader.h>
#include "MainComponent.h"
#include "CrashLog.h"
#include "SplashComponent.h"

class MainWindow : public juce::DocumentWindow
{
public:
    MainWindow(const juce::String& name)
#if JUCE_IOS
        : DocumentWindow(name, juce::Colours::black, 0)
#else
        : DocumentWindow(name, juce::Colours::black, DocumentWindow::allButtons)
#endif
    {
#if JUCE_IOS
        setTitleBarHeight(0);
        setFullScreen(true);
#else
        setUsingNativeTitleBar(false);
        setResizable(true, true);
#endif

        // Show splash first, then load main content
        splash = std::make_unique<SplashComponent>();
        splash->onFinished = [this] {
            juce::MessageManager::callAsync([this] {
                splash = nullptr;
                setContentOwned(new MainComponent(), false);
#if JUCE_IOS
                setFullScreen(true);
#endif
            });
        };

        setContentNonOwned(splash.get(), false);

#if JUCE_IOS
        setVisible(true);
        setFullScreen(true);
#else
        centreWithSize(1280, 800);
        setVisible(true);
#endif
    }

    void closeButtonPressed() override
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }

private:
    std::unique_ptr<SplashComponent> splash;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
};

class SequencerApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return "Legion Stage"; }
    const juce::String getApplicationVersion() override { return "1.1.0"; }
    bool moreThanOneInstanceAllowed() override           { return false; }

    void initialise(const juce::String& /*commandLine*/) override
    {
        CrashLog::install();

#if JUCE_IOS
        juce::Desktop::getInstance().setOrientationsEnabled(
            juce::Desktop::rotatedClockwise | juce::Desktop::rotatedAntiClockwise);
#endif

#ifdef _WIN32
        juce::File oldExe("C:\\Program Files\\Legion Stage\\Legion Stage.exe.old");
        if (oldExe.existsAsFile())
            oldExe.deleteFile();
#endif

        mainWindow = std::make_unique<MainWindow>(getApplicationName());
    }

    void shutdown() override
    {
        mainWindow = nullptr;
    }

    void systemRequestedQuit() override
    {
        quit();
    }

private:
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION(SequencerApplication)
