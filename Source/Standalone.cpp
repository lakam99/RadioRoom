#include "Processor.h"
#include "Editor.h"
#include "Tests.h"
#include "Updater.h"
#if JUCE_WINDOWS
#define NOMINMAX
#include <windows.h>
#include <dwmapi.h>
#endif

class MainPanel final:public juce::AudioAppComponent,private juce::Timer {
public:
    RadioProcessor processor;
    MainPanel(bool manualUpdate=false):pendingManual(manualUpdate){
        processor.standalone.store(true);formats.registerBasicFormats();readAhead.startThread();
        juce::PropertiesFile::Options options;options.applicationName="RadioRoom";options.filenameSuffix="settings";options.folderName="RadioRoom";options.osxLibrarySubFolder="Application Support";prefs=std::make_unique<juce::PropertiesFile>(options);
        juce::MemoryBlock state;if(state.fromBase64Encoding(prefs->getValue("state")))processor.setStateInformation(state.getData(),(int)state.getSize());
        editor=std::make_unique<RadioEditor>(processor);
        editor->onUpdates=[this]{showUpdatesMenu();};editor->updateProgress=[this]{return std::pair<double,juce::String>{updater.progress,updater.stage()};};
        editor->isLoaded=[this]{return source!=nullptr;};
        editor->onOpen=[this]{chooser=std::make_unique<juce::FileChooser>("Open a mix",juce::File{},formats.getWildcardForAllFormats());chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[safe=juce::Component::SafePointer<MainPanel>(this)](const juce::FileChooser& fc){if(safe&&fc.getResult().existsAsFile())safe->load(fc.getResult());});};
        editor->onPlay=[this]{if(source){transport.start();processor.playing.store(true);}};editor->onStop=[this]{transport.stop();processor.playing.store(false);};editor->onRewind=[this]{transport.setPosition(0);};editor->onLoop=[this]{looping=!looping;if(source)source->setLooping(looping);};editor->isLooping=[this]{return looping;};
        editor->fileStatus=[this]{return source?juce::String(transport.isPlaying()?"PLAY / ":"STOP / ")+filename+" / "+juce::String(transport.getCurrentPosition(),1)+" S":"OPEN A MIX TO AUDITION";};
        editor->onDevices=[this]{auto selector=std::make_unique<juce::AudioDeviceSelectorComponent>(deviceManager,0,0,2,2,false,false,true,false);selector->setSize(480,420);juce::DialogWindow::LaunchOptions opt;opt.content.setOwned(selector.release());opt.dialogTitle="RadioRoom audio output";opt.dialogBackgroundColour=juce::Colours::darkgrey;opt.useNativeTitleBar=true;opt.escapeKeyTriggersCloseButton=true;opt.launchAsync();};
        addAndMakeVisible(*editor);setSize(editor->getWidth(),editor->getHeight());setAudioChannels(0,2);
        auto updateResult=juce::File(juce::SystemStats::getEnvironmentVariable("LOCALAPPDATA",{})).getChildFile("RadioRoom/last-update-result.json");
        if(updateResult.existsAsFile()&&updateResult.getSize()<65536){auto result=juce::JSON::parse(updateResult);updateResult.deleteFile();if(result.isObject()&&!(bool)result["passed"])juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"RadioRoom update was not installed",result["error"].toString()+"\nYour previous installation was retained.");}
        startTimer(1800);
    }
    ~MainPanel() override {stopTimer();shutdownAudio();transport.setSource(nullptr);source.reset();readAhead.stopThread(2000);juce::MemoryBlock state;processor.getStateInformation(state);prefs->setValue("state",state.toBase64Encoding());prefs->saveIfNeeded();editor.reset();}
    void prepareToPlay(int size,double sr) override{transport.prepareToPlay(size,sr);processor.prepareToPlay(sr,size);}
    void releaseResources() override{transport.releaseResources();processor.releaseResources();}
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& info) override{transport.getNextAudioBlock(info);juce::AudioBuffer<float> view(info.buffer->getArrayOfWritePointers(),std::min(2,info.buffer->getNumChannels()),info.startSample,info.numSamples);juce::MidiBuffer midi;processor.processBlock(view,midi);}
    void resized() override{if(editor)editor->setBounds(getLocalBounds());}
private:
    RadioUpdater updater;bool pendingManual=false;
    void timerCallback() override {startTimer(3600000);bool manual=pendingManual;pendingManual=false;auto last=prefs->getValue("lastUpdateCheck","0").getLargeIntValue();if(manual||(prefs->getBoolValue("automaticUpdates",true)&&juce::Time::getCurrentTime().toMilliseconds()-last>=86400000))checkUpdates(manual,false);}
    void showUpdatesMenu(){juce::PopupMenu m;m.addItem(1,"Check for updates...");m.addItem(2,"Automatically check for updates",true,prefs->getBoolValue("automaticUpdates",true));m.addItem(3,"Open GitHub releases");m.showMenuAsync(juce::PopupMenu::Options{}.withTargetComponent(editor.get()),[safe=juce::Component::SafePointer<MainPanel>(this)](int n){if(!safe)return;if(n==1)safe->checkUpdates(true,false);if(n==2){safe->prefs->setValue("automaticUpdates",!safe->prefs->getBoolValue("automaticUpdates",true));safe->prefs->saveIfNeeded();}if(n==3)juce::URL("https://github.com/lakam99/RadioRoom/releases").launchInDefaultBrowser();});}
    void checkUpdates(bool manual,bool prepare){
        if(updater.busy())return;editor->setUpdateBusy(true);
        updater.request(prepare,[safe=juce::Component::SafePointer<MainPanel>(this),manual,prepare](juce::var result){if(!safe)return;safe->editor->setUpdateBusy(false);
            if(!(bool)result["passed"]){if(manual)juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"RadioRoom updates",result["error"].toString());return;}
            safe->prefs->setValue("lastUpdateCheck",juce::String(juce::Time::getCurrentTime().toMilliseconds()));safe->prefs->saveIfNeeded();
            if(!(bool)result["available"]){if(manual)juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,"RadioRoom updates","RadioRoom " RADIOROOM_VERSION " is current. No newer stable release is available.");return;}
            auto options=juce::MessageBoxOptions{}.withTitle("RadioRoom "+result["version"].toString()).withMessage(prepare?"Download verified. Save and close DAWs using RadioRoom before installing. Playback must be stopped.":"A newer stable release is available. Download and verify it?").withButton(prepare?"Install and restart":"Download update",1).withButton("Later",0);
            juce::AlertWindow::showAsync(options,[safe,prepare,result](int choice){if(!safe||choice!=1)return;if(!prepare){safe->checkUpdates(true,true);return;}if(safe->transport.isPlaying()){juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,"RadioRoom updates","Stop playback before installing.");return;}juce::String error;if(RadioUpdater::installAndRestart(result,error))juce::JUCEApplication::getInstance()->systemRequestedQuit();else juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"RadioRoom updates",error);});
        });
    }
    juce::AudioFormatManager formats;juce::TimeSliceThread readAhead{"RadioRoom file read-ahead"};juce::AudioTransportSource transport;
    std::unique_ptr<juce::AudioFormatReaderSource> source;std::unique_ptr<RadioEditor> editor;
    std::unique_ptr<juce::PropertiesFile> prefs;std::unique_ptr<juce::FileChooser> chooser;juce::String filename;bool looping=false;
    void load(const juce::File& f){std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(f));if(!reader){filename="UNSUPPORTED AUDIO FILE";return;}double rate=reader->sampleRate;transport.stop();transport.setSource(nullptr);source=std::make_unique<juce::AudioFormatReaderSource>(reader.release(),true);source->setLooping(looping);transport.setSource(source.get(),65536,&readAhead,rate);filename=f.getFileName();processor.playing.store(false);}
};
class Window final:public juce::DocumentWindow,private juce::DarkModeSettingListener {
public:
    Window(bool updates):DocumentWindow("RadioRoom",juce::Colours::darkgrey,allButtons){setUsingNativeTitleBar(true);setContentOwned(new MainPanel(updates),true);setIcon(RasterAssets::get().icon);centreWithSize(getWidth(),getHeight());setVisible(true);juce::Desktop::getInstance().addDarkModeSettingListener(this);darkModeSettingChanged();}
    ~Window() override{juce::Desktop::getInstance().removeDarkModeSettingListener(this);clearContentComponent();}
    void closeButtonPressed() override{juce::JUCEApplication::getInstance()->systemRequestedQuit();}
    void darkModeSettingChanged() override{bool dark=juce::Desktop::getInstance().isDarkModeActive();setBackgroundColour(dark?juce::Colour(0xff25282b):juce::Colour(0xffeeeeee));
#if JUCE_WINDOWS
        if(auto* peer=getPeer()){BOOL enabled=dark;DwmSetWindowAttribute((HWND)peer->getNativeHandle(),20,&enabled,sizeof(enabled));}
#endif
    }
};
class App final:public juce::JUCEApplication {
public:
    const juce::String getApplicationName() override{return "RadioRoom";}
    const juce::String getApplicationVersion() override{return RADIOROOM_VERSION;}
    bool moreThanOneInstanceAllowed() override{return true;}
    std::unique_ptr<Window> window;
    void initialise(const juce::String& command) override{
        auto args=juce::StringArray::fromTokens(command,true);args.trim();args.removeEmptyStrings();for(auto& a:args)a=a.unquoted();
        auto value=[&](const juce::String& key){int at=args.indexOf(key);return at>=0&&at+1<args.size()?juce::File::getCurrentWorkingDirectory().getChildFile(args[at+1]):juce::File{};};
        try {
            if(args.contains("--updater-test")){bool callback=false;double elapsed=0;{auto updater=std::make_unique<RadioUpdater>();if(!updater->request(false,[&](juce::var){callback=true;}))throw std::runtime_error("Updater test could not start helper");auto start=juce::Time::getMillisecondCounterHiRes();updater.reset();elapsed=juce::Time::getMillisecondCounterHiRes()-start;}juce::MessageManager::getInstance()->runDispatchLoopUntil(150);auto* r=new juce::DynamicObject;r->setProperty("passed",elapsed<250&&!callback);r->setProperty("cancellation_ms",elapsed);r->setProperty("callback_suppressed_after_destruction",!callback);value("--updater-test").replaceWithText(juce::JSON::toString(juce::var(r)));setApplicationReturnValue(elapsed<250&&!callback?0:1);quit();return;}
            if(args.contains("--check-updates")){RadioUpdater updater;bool done=false;juce::var report;updater.request(false,[&](juce::var r){report=r;done=true;});auto end=juce::Time::getMillisecondCounterHiRes()+40000;while(!done&&juce::Time::getMillisecondCounterHiRes()<end)juce::MessageManager::getInstance()->runDispatchLoopUntil(25);if(!done)throw std::runtime_error("Update check timed out");value("--check-updates").replaceWithText(juce::JSON::toString(report));setApplicationReturnValue((bool)report["passed"]?0:1);quit();return;}
            if(args.contains("--test")){auto report=runTests();value("--test").replaceWithText(juce::JSON::toString(report));setApplicationReturnValue((bool)report["passed"]?0:1);quit();return;}
            if(args.contains("--host")){auto report=hostedTests(value("--host"));value("--report").replaceWithText(juce::JSON::toString(report));setApplicationReturnValue((bool)report["passed"]?0:1);quit();return;}
            if(args.contains("--process")){int index=args.indexOf("--preset");int preset=index>=0&&index+1<args.size()?args[index+1].getIntValue():1;processFile(value("--process"),value("--output"),preset);quit();return;}
            if(args.contains("--snapshot")){RadioProcessor p;p.standalone.store(true);int index=args.indexOf("--skin");if(index>=0&&index+1<args.size())p.skin.store(juce::jlimit(0,2,args[index+1].getIntValue()));std::unique_ptr<juce::AudioProcessorEditor> e(p.createEditor());auto image=e->createComponentSnapshot(e->getLocalBounds(),true,1);juce::MemoryOutputStream bytes;juce::PNGImageFormat png;if(!png.writeImageToStream(image,bytes)||!value("--snapshot").replaceWithData(bytes.getData(),bytes.getDataSize()))throw std::runtime_error("Snapshot failed");quit();return;}
            window=std::make_unique<Window>(args.contains("--updates"));
        }catch(const std::exception& e){juce::File::getCurrentWorkingDirectory().getChildFile("radioroom-error.txt").replaceWithText(e.what());setApplicationReturnValue(2);quit();}
    }
    void shutdown() override{window.reset();}
};
START_JUCE_APPLICATION(App)
