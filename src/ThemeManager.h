#pragma once

#include <JuceHeader.h>
#include "DawLookAndFeel.h"
#include "WalkmanLookAndFeel.h"
#include "GlassLookAndFeel.h"
#include "NoirLookAndFeel.h"
#include "TerminalLookAndFeel.h"
#include "JournalLookAndFeel.h"
#include "LinenLookAndFeel.h"
#include "IceLookAndFeel.h"
#include "IndigoLookAndFeel.h"
#include "FrostLookAndFeel.h"
#include "KeystageLookAndFeel.h"
#include "BlueprintLookAndFeel.h"

class ThemeManager
{
public:
    enum Theme
    {
        Walkman = 0,
        Glass,
        Noir,
        Terminal,
        Journal,
        Linen,
        Ice,
        Indigo,
        Frost,
        Keystage,
        Blueprint,
        NumThemes
    };

    ThemeManager()
    {
        themes[Walkman]  = std::make_unique<WalkmanLookAndFeel>();
        themes[Glass]    = std::make_unique<GlassLookAndFeel>();
        themes[Noir]     = std::make_unique<NoirLookAndFeel>();
        themes[Terminal] = std::make_unique<TerminalLookAndFeel>();
        themes[Journal]  = std::make_unique<JournalLookAndFeel>();
        themes[Linen]    = std::make_unique<LinenLookAndFeel>();
        themes[Ice]      = std::make_unique<IceLookAndFeel>();
        themes[Indigo]   = std::make_unique<IndigoLookAndFeel>();
        themes[Frost]    = std::make_unique<FrostLookAndFeel>();
        themes[Keystage]  = std::make_unique<KeystageLookAndFeel>();
        themes[Blueprint] = std::make_unique<BlueprintLookAndFeel>();
    }

    // Apply the given theme to a component tree
    void setTheme(Theme t, juce::Component* root)
    {
        currentTheme = t;
        if (root)
        {
            root->setLookAndFeel(getLookAndFeel());
            root->repaint();
            // Force all children to repaint with new theme
            repaintAll(root);
        }
    }

    Theme getCurrentTheme() const { return currentTheme; }

    DawLookAndFeel* getLookAndFeel() { return themes[currentTheme].get(); }
    const DawTheme& getColors() const { return themes[currentTheme]->getTheme(); }

    static juce::String getThemeName(Theme t)
    {
        switch (t)
        {
            case Walkman:  return "Walkman";
            case Glass:    return "Glass";
            case Noir:     return "Noir";
            case Terminal: return "Terminal";
            case Journal:  return "Journal";
            case Linen:    return "Linen";
            case Ice:       return "Ice";
            case Indigo:    return "Indigo";
            case Frost:     return "Frost";
            case Keystage:  return "Keystage";
            case Blueprint: return "Blueprint";
            default:        return "Unknown";
        }
    }

private:
    Theme currentTheme = Ice;
    std::unique_ptr<DawLookAndFeel> themes[NumThemes];

    void repaintAll(juce::Component* comp)
    {
        for (int i = 0; i < comp->getNumChildComponents(); ++i)
        {
            auto* child = comp->getChildComponent(i);
            child->setLookAndFeel(nullptr);  // inherit from parent
            child->repaint();
            repaintAll(child);
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ThemeManager)
};
