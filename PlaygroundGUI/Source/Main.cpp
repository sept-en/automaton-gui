/*
 * Automaton Playground
 * Copyright (c) 2019 The Automaton Authors.
 * Copyright (c) 2019 The automaton.network Authors.
 *
 * Automaton Playground is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * Automaton Playground is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with Automaton Playground.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "../JuceLibraryCode/JuceHeader.h"
#include "MainComponent.h"
#include "Data/AutomatonContractData.h"

#include <curl/curl.h>

#include <sol.hpp>

class EmbeddedFonts {
 private:
  Font play;
  Font playBold;

 public:
  EmbeddedFonts() {
    play = Font(Typeface::createSystemTypefaceFor(BinaryData::PlayRegular_ttf, BinaryData::PlayRegular_ttfSize));
    playBold = Font(Typeface::createSystemTypefaceFor(BinaryData::PlayBold_ttf, BinaryData::PlayBold_ttfSize));
  }

  const Font& getPlay() { return play; }
  const Font& getPlayBold() { return playBold; }
};


//==============================================================================
class PlaygroundGUIApplication: public JUCEApplication {
 public:
  //==============================================================================
  PlaygroundGUIApplication() {}

  const String getApplicationName() override       { return ProjectInfo::projectName; }
  const String getApplicationVersion() override    { return ProjectInfo::versionString; }
  bool moreThanOneInstanceAllowed() override       { return false; }

  //==============================================================================
  void initialise(const String& commandLine) override {
    curl_global_init(CURL_GLOBAL_ALL);

    mainWindow.reset(new MainWindow(getApplicationName()));

    // const Font& fontPlay = fonts.getPlay();
    // typefacePlay = LookAndFeel::getDefaultLookAndFeel().getTypefaceForFont(fontPlay);
    // LookAndFeel::getDefaultLookAndFeel().setDefaultSansSerifTypeface(typefacePlay);
  }

  void shutdown() override {
    AutomatonContractData::deleteInstance();
    Config::deleteInstance();

    mainWindow = nullptr;

    curl_global_cleanup();
  }

  //==============================================================================
  void systemRequestedQuit() override {
    // This is called when the app is being asked to quit: you can ignore this
    // request and let the app carry on running, or call quit() to allow the app to close.
    quit();
  }

  void anotherInstanceStarted(const String& commandLine) override {
      // When another instance of the app is launched while this one is running,
      // this method is invoked, and the commandLine parameter tells you what
      // the other instance's command-line arguments were.
  }

  //==============================================================================
  /*
      This class implements the desktop window that contains an instance of
      our DemoMainComponent class.
  */
  class MainWindow    : public DocumentWindow {
   public:
    LookAndFeel_V4 lnf;

    MainWindow(String name):
      DocumentWindow(name,
                     Desktop::getInstance()
                       .getDefaultLookAndFeel()
                       .findColour(ResizableWindow::backgroundColourId),
                     DocumentWindow::allButtons) {
      LookAndFeel::setDefaultLookAndFeel(&lnf);

      setUsingNativeTitleBar(true);
      setContentOwned(new DemosMainComponent(), true);

      setFullScreen(true);
      setResizable(true, true);
      setResizeLimits(1200, 800, 10000, 10000);

      setVisible(true);
    }

    void closeButtonPressed() override {
        // This is called when the user tries to close this window. Here, we'll just
        // ask the app to quit when this happens, but you can change this to do
        // whatever you need.
        JUCEApplication::getInstance()->systemRequestedQuit();
    }

    /* Note: Be careful if you override any DocumentWindow methods - the base
       class uses a lot of them, so by overriding you might break its functionality.
       It's best to do all your work in your content component instead, but if
       you really have to override any DocumentWindow methods, make sure your
       subclass also calls the superclass's method.
    */

   private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
  };

 private:
  std::unique_ptr<MainWindow> mainWindow;
  EmbeddedFonts fonts;
  Typeface::Ptr typefacePlay;
};

//==============================================================================
// This macro generates the main() routine that launches the app.
START_JUCE_APPLICATION(PlaygroundGUIApplication)
