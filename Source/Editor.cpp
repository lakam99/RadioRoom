#include "Editor.h"

namespace {
void text(juce::Graphics& g,const juce::String& s,juce::Rectangle<int> b,juce::Colour c,float size=11){g.setColour(c);g.setFont(juce::Font(juce::FontOptions("Arial",size,juce::Font::bold)));g.drawText(s,b,juce::Justification::centred);}
}
void RadioLook::drawRotarySlider(juce::Graphics& g,int x,int y,int w,int h,float position,float start,float end,juce::Slider&){
    auto area=juce::Rectangle<float>((float)x,(float)y,(float)w,(float)h).reduced(14);float radius=std::min(area.getWidth(),area.getHeight())*.5f,cx=area.getCentreX(),cy=area.getCentreY();
    for(int i=0;i<=10;++i){float a=start+(end-start)*i/10;g.setColour(ink().withAlpha(.65f));g.drawLine(cx+std::sin(a)*(radius+4),cy-std::cos(a)*(radius+4),cx+std::sin(a)*(radius+9),cy-std::cos(a)*(radius+9),i%5==0?1.5f:.75f);}
    g.setOpacity(1);g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);g.drawImage(RasterAssets::get().knob,{cx-radius,cy-radius,radius*2,radius*2},juce::RectanglePlacement::stretchToFit);
    float a=start+position*(end-start);g.setColour(juce::Colour(0xfffff3d6));g.drawLine(cx+std::sin(a)*radius*.4f,cy-std::cos(a)*radius*.4f,cx+std::sin(a)*radius*.8f,cy-std::cos(a)*radius*.8f,2.6f);
}
void RadioLook::drawButtonBackground(juce::Graphics& g,juce::Button& b,const juce::Colour&,bool over,bool down){
    auto r=b.getLocalBounds().toFloat().reduced(1);auto id=b.getComponentID();int sprite=id=="play"?0:id=="stop"?2:id=="rewind"?4:id=="loop"?3:-1;
    g.setOpacity(b.isEnabled()?1.f:.36f);g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
    if(sprite>=0)g.drawImage(RasterAssets::get().transport[(size_t)sprite],r,juce::RectanglePlacement::centred);
    else g.drawImage(RasterAssets::get().buttons[(size_t)(down||b.getToggleState())],r,juce::RectanglePlacement::stretchToFit);
    if(over||b.getToggleState()){g.setColour(juce::Colour(0xff65e7d9).withAlpha(b.getToggleState()?.18f:.07f));g.fillRoundedRectangle(r.reduced(5),3);}
}
void RadioLook::drawButtonText(juce::Graphics& g,juce::TextButton& b,bool,bool){if(b.getComponentID().isEmpty())text(g,b.getButtonText(),b.getLocalBounds(),juce::Colour(0xffffeed1),10);}
RadioEditor::RadioEditor(RadioProcessor& proc):AudioProcessorEditor(proc),p(proc){
    look.skin=p.skin.load();setLookAndFeel(&look);(void)RasterAssets::get();
    for(int i=0;i<ControlCount;++i){auto& k=knobs[(size_t)i];k.setSliderStyle(juce::Slider::RotaryVerticalDrag);k.setRotaryParameters(juce::MathConstants<float>::pi*1.22f,juce::MathConstants<float>::pi*2.78f,true);k.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);k.setName(names[i]);k.setDoubleClickReturnValue(true,defaults[i]);k.setTooltip(descriptions[i]);addAndMakeVisible(k);attachments[(size_t)i]=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.state,ids[i],k);}
    for(auto* b:{&preset,&skinButton,&bypass,&open,&play,&stop,&rewind,&loop,&device,&updates})addAndMakeVisible(b);
    updates.onClick=[this]{if(onUpdates){onUpdates();return;}auto exe=juce::File(juce::SystemStats::getEnvironmentVariable("LOCALAPPDATA",{})).getChildFile("Programs/RadioRoom/RadioRoom.exe");if(!exe.existsAsFile()||!exe.startAsProcess("--updates"))juce::URL("https://github.com/lakam99/RadioRoom/releases").launchInDefaultBrowser();};
    updates.setTooltip("Check GitHub releases in the RadioRoom standalone updater. Updating both formats requires the DAW to release this plugin.");
    bypass.setClickingTogglesState(true);bypassAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.state,"bypass",bypass);
    bypass.setTooltip("Latency-aligned bypass. The original passes at its original level.");
    preset.onClick=[this]{juce::PopupMenu menu;menu.addItem(1,"1928 / Parlour wireless");menu.addItem(2,"1952 / Warm broadcast");menu.addItem(3,"One room / Open ensemble");menu.showMenuAsync(juce::PopupMenu::Options{}.withTargetComponent(&preset),[safe=juce::Component::SafePointer<RadioEditor>(this)](int n){if(safe&&n>0)safe->p.applyPreset(n-1);});};
    skinButton.onClick=[this]{p.skin.store((p.skin.load()+1)%3);look.skin=p.skin.load();repaint();};
    open.onClick=[this]{if(onOpen)onOpen();};play.onClick=[this]{if(onPlay)onPlay();};stop.onClick=[this]{if(onStop)onStop();};rewind.onClick=[this]{if(onRewind)onRewind();};loop.onClick=[this]{if(onLoop)onLoop();};device.onClick=[this]{if(onDevices)onDevices();};
    play.setComponentID("play");stop.setComponentID("stop");rewind.setComponentID("rewind");loop.setComponentID("loop");
    play.setTooltip("Play the loaded mix");stop.setTooltip("Stop playback");rewind.setTooltip("Return to the beginning");loop.setTooltip("Loop the loaded mix");open.setTooltip("Load an audio file in standalone");
    for(auto* b:{&open,&play,&stop,&rewind,&loop,&device})b->setVisible(p.standalone.load());
    for(auto* b:{&play,&stop,&rewind,&loop})b->setEnabled(false);
    setSize(1100,760);setResizable(false,false);startTimerHz(24);
}
RadioEditor::~RadioEditor(){stopTimer();setLookAndFeel(nullptr);}
void RadioEditor::timerCallback(){input=std::max(p.inputMeter.load(),input*.86f);output=std::max(p.outputMeter.load(),output*.86f);reduction=std::max(p.reductionMeter.load(),reduction*.85f);if(isLooping)loop.setToggleState(isLooping(),juce::dontSendNotification);if(isLoaded)for(auto* b:{&play,&stop,&rewind,&loop})b->setEnabled(isLoaded());repaint();}
void RadioEditor::resized(){
    for(int i=0;i<8;++i){cells[(size_t)i]={45+i*127,239,121,171};knobs[(size_t)i].setBounds(cells[(size_t)i].withTrimmedTop(21).withTrimmedBottom(31));}
    constexpr int lower[]{Room,Decay,Distance,Width,Crackle,Mix,Output};
    for(int j=0;j<7;++j){int i=lower[j];cells[(size_t)i]={49+j*143,455,136,167};knobs[(size_t)i].setBounds(cells[(size_t)i].withTrimmedTop(21).withTrimmedBottom(31));}
    preset.setBounds(52,654,90,32);skinButton.setBounds(52,697,90,30);bypass.setBounds(943,657,103,41);
    open.setBounds(479,659,98,34);rewind.setBounds(589,656,52,39);play.setBounds(642,656,52,39);stop.setBounds(695,656,52,39);loop.setBounds(748,656,52,39);device.setBounds(813,659,90,34);
    updates.setBounds(943,703,103,27);
}
void RadioEditor::meter(juce::Graphics& g,juce::Rectangle<float> r,float value,bool gr,const juce::String& name){
    auto& a=RasterAssets::get();g.setOpacity(1);g.drawImage(a.meter,r,juce::RectanglePlacement::stretchToFit);
    auto paper=juce::Rectangle<float>(r.getX()+a.meterPaper.getX()*r.getWidth(),r.getY()+a.meterPaper.getY()*r.getHeight(),a.meterPaper.getWidth()*r.getWidth(),a.meterPaper.getHeight()*r.getHeight());
    float cx=paper.getCentreX(),cy=paper.getBottom()-8,rad=paper.getHeight()*.60f;
    for(int i=0;i<=20;++i){float angle=-.9f+i*.09f;float x=cx+std::sin(angle)*rad,y=cy-std::cos(angle)*rad;
        g.setColour(juce::Colour(!gr&&i>=19?0xff96351e:0xff3e3426));g.drawLine(x,y,cx+std::sin(angle)*(rad-(i%5==0?5:2.5f)),cy-std::cos(angle)*(rad-(i%5==0?5:2.5f)),.8f);
        if(i%5==0)text(g,juce::String(gr?i*6/5:-48+i*12/5),{(int)(cx+std::sin(angle)*(rad+11))-12,(int)(cy-std::cos(angle)*(rad+11))-5,24,11},juce::Colour(0xff493b29),9);}
    float pos=gr?value/24.f:(juce::Decibels::gainToDecibels(value,-60.f)+48)/48;
    float angle=-.9f+juce::jlimit(0.f,1.f,pos)*1.8f;g.setColour(juce::Colour(0xff8e341f));g.drawLine(cx,cy,cx+std::sin(angle)*(rad-3),cy-std::cos(angle)*(rad-3),1.5f);
    g.setColour(juce::Colour(0xff4c3524));g.fillEllipse(cx-2,cy-2,4,4);
    text(g,name,{(int)r.getX(),(int)r.getBottom()+3,(int)r.getWidth(),17},look.ink(),9);
}
void RadioEditor::paint(juce::Graphics& g){
    auto ink=look.ink();g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);g.drawImage(RasterAssets::get().panels[(size_t)look.skin],getLocalBounds().toFloat(),juce::RectanglePlacement::stretchToFit);
    text(g,"RADIOROOM",{43,40,365,48},ink,38);text(g,"ONE ENSEMBLE. ONE ROOM. ON AIR.",{46,91,358,23},ink.withAlpha(.8f),12);text(g,"LAKAM AUDIO   /   MIX BROADCAST SYSTEM",{46,124,358,19},ink.withAlpha(.65f),10);
    meter(g,{434,36,176,113},input,false,"INPUT / dBFS");meter(g,{619,36,176,113},reduction,true,"REDUCTION / dB");meter(g,{804,36,176,113},output,false,"OUTPUT / dBFS");
    drawVfdHousing(g,{50,177,995,38});status.draw(g,p.state.getRawParameterValue("bypass")->load()>.5f?"BYPASSED / ORIGINAL MIX":output>1?"OUTPUT OVER 0 DBFS / LOWER OUTPUT":"SHARED ROOM > LINKED GLUE > TUBES > RADIO SPEAKER",{67,185,958,22},1.5f,false);
    if(updateBusy){auto progress=updateProgress?updateProgress():std::pair<double,juce::String>{-1,"CHECKING RELEASES"};g.setColour(juce::Colour(0xff071615));g.fillRect(66,183,962,28);status.draw(g,progress.second+(progress.first>=0?" / "+juce::String((int)std::round(progress.first*100))+" %":""),{67,184,958,19},1.4f,false);for(int i=0;i<96;++i){bool lit=progress.first>=0?i<progress.first*96:(i+(int)(juce::Time::getMillisecondCounter()/90))%18<6;g.setColour(juce::Colour(0xff71ebd7).withAlpha(lit?.9f:.12f));g.fillRect(67+i*10,206,7,3);}}
    text(g,"BROADCAST CHARACTER",{52,216,203,20},ink.withAlpha(.75f),10);text(g,"THE COMMON MICROPHONE",{52,427,223,20},ink.withAlpha(.75f),10);
    g.setColour(ink.withAlpha(.22f));g.drawHorizontalLine(422,52,1045);g.drawHorizontalLine(640,52,1045);
    for(int i=0;i<ControlCount;++i){auto cell=cells[(size_t)i];text(g,juce::String(names[i]).toUpperCase(),cell.withHeight(21),ink,11);auto display=cell.withTrimmedTop(cell.getHeight()-29).reduced(9,0).toFloat();drawVfdHousing(g,display);
        float v=(float)knobs[(size_t)i].getValue();juce::String s=i==Era?juce::String((int)v):i==Body||i==Presence||i==Input||i==Output?juce::String(v,1)+" DB":i==Decay?juce::String(v,2)+" S":juce::String((int)std::round(v*100))+" %";values[(size_t)i].draw(g,s,display.reduced(9,4),1.25f,false);}
    juce::String presetName="CUSTOM / ERA "+juce::String((int)knobs[Era].getValue());const char* presetNames[]{"1928 / PARLOUR WIRELESS","1952 / WARM BROADCAST","ONE ROOM / OPEN ENSEMBLE"};
    for(int n=0;n<3;++n){auto v=RadioProcessor::presetValues(n);bool match=true;for(int i=0;i<ControlCount;++i)match&=std::abs((float)knobs[(size_t)i].getValue()-v[(size_t)i])<.002f;if(match)presetName=presetNames[n];}
    drawVfdHousing(g,{156,655,300,32});details.draw(g,presetName,{168,660,275,22},1.25f,false);
    drawVfdHousing(g,{156,698,300,29});const char* finish[]{"IVORY / WALNUT","GRAPHITE","WALNUT"};finishLine.draw(g,finish[look.skin],{168,702,275,21},1.2f,false);
    drawVfdHousing(g,{479,701,450,28});juce::String source=p.standalone.load()?(fileStatus?fileStatus():"OPEN A MIX TO AUDITION"):"STEREO LINKED / HOST AUTOMATION / SHARED ROOM";sourceLine.draw(g,source,{492,705,425,20},1.15f,true);
    if(!p.standalone.load())text(g,"FEED THE ENTIRE MIX INTO ONE INSTANCE",{480,659,420,32},ink.withAlpha(.75f),11);
}
