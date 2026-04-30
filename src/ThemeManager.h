#pragma once

#include <JuceHeader.h>
#include "DawLookAndFeel.h"
#include "KeystageLookAndFeel.h"
#include "AlcantaraLookAndFeel.h"
#include "IoniqLookAndFeel.h"
#include "IoniqDarkLookAndFeel.h"
#include "IoniqForestLookAndFeel.h"
#include "CyberpunkLookAndFeel.h"
#include "OxideLookAndFeel.h"
#include "LiquidGlassLookAndFeel.h"
#include "LiquidGlassLightLookAndFeel.h"
#include "GlassOverlayLookAndFeel.h"
#include "OceanGlassLookAndFeel.h"
#include "LaboratoryLookAndFeel.h"
#include "LaunchkeyLookAndFeel.h"
#include "LaunchkeyDarkLookAndFeel.h"
#include "LaunchkeyOledLookAndFeel.h"

class ThemeManager
{
public:
    enum Theme
    {
        Keystage = 0,
        Alcantara,
        Ioniq,
        IoniqDark,
        IoniqForest,  // kept for save compatibility but hidden from UI
        Cyberpunk,
        LiquidGlass,
        LiquidGlassLight,
        GlassOverlay,
        OceanGlass,
        Laboratory,
        Launchkey,
        LaunchkeyDark,
        LaunchkeyOled,
        NumThemes
    };

    ThemeManager()
    {
        themes[Keystage]  = std::make_unique<KeystageLookAndFeel>();
        themes[Alcantara] = std::make_unique<AlcantaraLookAndFeel>();
        themes[Ioniq]     = std::make_unique<IoniqLookAndFeel>();
        themes[IoniqDark]   = std::make_unique<IoniqDarkLookAndFeel>();
        themes[IoniqForest] = std::make_unique<IoniqForestLookAndFeel>();
        themes[Cyberpunk]   = std::make_unique<CyberpunkLookAndFeel>();
        themes[LiquidGlass] = std::make_unique<LiquidGlassLookAndFeel>();
        themes[LiquidGlassLight] = std::make_unique<LiquidGlassLightLookAndFeel>();
        themes[GlassOverlay] = std::make_unique<GlassOverlayLookAndFeel>();
        themes[OceanGlass] = std::make_unique<OceanGlassLookAndFeel>();
        themes[Laboratory] = std::make_unique<LaboratoryLookAndFeel>();
        themes[Launchkey] = std::make_unique<LaunchkeyLookAndFeel>();
        themes[LaunchkeyDark] = std::make_unique<LaunchkeyDarkLookAndFeel>();
        themes[LaunchkeyOled] = std::make_unique<LaunchkeyOledLookAndFeel>();
    }

    // Apply the given theme to a component tree
    void setTheme(Theme t, juce::Component* root)
    {
        themes[currentTheme]->invalidateCaches();
        currentTheme = t;
        themes[currentTheme]->invalidateCaches();
        if (root)
        {
            root->setLookAndFeel(getLookAndFeel());
            root->repaint();
            // Force all children to repaint with new theme
            repaintAll(root);
        }
    }

    Theme getCurrentTheme() const { return currentTheme; }

    bool isGlassOverlay() const { return currentTheme == GlassOverlay || currentTheme == OceanGlass || currentTheme == LiquidGlassLight || currentTheme == Laboratory; }
    DawLookAndFeel* getLookAndFeel() { return themes[currentTheme].get(); }
    const DawLookAndFeel* getLookAndFeel() const { return themes[currentTheme].get(); }
    const DawTheme& getColors() const { return themes[currentTheme]->getTheme(); }

    static juce::String getThemeName(Theme t)
    {
        switch (t)
        {
            case Keystage:  return "Keystage";
            case Alcantara: return "Alcantara";
            case Ioniq:     return "Ioniq";
            case IoniqDark:   return "Ioniq Dark";
            case IoniqForest: return "Ioniq Forest";
            case Cyberpunk:   return "Cyberpunk";
            case LiquidGlass:      return "Liquid Glass";
            case LiquidGlassLight: return "Glass Light";
            case GlassOverlay:     return "Glass Overlay";
            case OceanGlass:       return "Oceania";
            case Laboratory:       return "Laboratory";
            case Launchkey:        return "Launchkey";
            case LaunchkeyDark:    return "Launchkey Dark";
            case LaunchkeyOled:    return "Launchkey OLED";
            default:               return "Unknown";
        }
    }

private:
    Theme currentTheme = Ioniq;
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
