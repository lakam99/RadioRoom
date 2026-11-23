#include "Updater.h"
#include <cmath>
#if JUCE_WINDOWS
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#endif
namespace {
juce::File appDirectory(){return juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();}
juce::File powershell(){return juce::File::getSpecialLocation(juce::File::windowsSystemDirectory).getChildFile("WindowsPowerShell/v1.0/powershell.exe");}
juce::var failure(const juce::String& s){auto* r=new juce::DynamicObject;r->setProperty("passed",false);r->setProperty("error",s);return juce::var(r);}
}
RadioUpdater::~RadioUpdater(){stopTimer();callback={};if(child)child->kill();child.reset();clearFiles();}
void RadioUpdater::clearFiles(){if(report!=juce::File{})report.deleteFile();if(progressFile!=juce::File{}){progressFile.deleteFile();juce::File(progressFile.getFullPathName()+".tmp").deleteFile();juce::File(progressFile.getFullPathName()+".previous").deleteFile();}}
juce::String RadioUpdater::stage() const {const char* s[]{"CHECKING RELEASES","DOWNLOADING","VERIFYING","EXTRACTING","CHECKING FILES","READY"};return s[juce::jlimit(0,5,phase)];}
bool RadioUpdater::request(bool prepare,std::function<void(juce::var)> cb){
    if(child)return false;progress=-1;phase=0;callback=std::move(cb);
    auto helper=appDirectory().getChildFile("RadioRoom-Update.ps1");
    report=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("RadioRoom-update-"+juce::Uuid{}.toString(),".json",false);progressFile=juce::File(report.getFullPathName()+".progress.json");
    child=std::make_unique<juce::ChildProcess>();
    juce::StringArray args{powershell().getFullPathName(),"-NoProfile","-NonInteractive","-ExecutionPolicy","Bypass","-File",helper.getFullPathName(),"-Mode",prepare?"Prepare":"Check","-CurrentVersion",RADIOROOM_VERSION,"-Report",report.getFullPathName()};
    if(!helper.existsAsFile()||!child->start(args,0)){child.reset();clearFiles();auto completion=std::move(callback);if(completion)completion(failure("Reinstall the complete RadioRoom package: the updater helper could not start."));return false;}
    deadline=juce::Time::getMillisecondCounterHiRes()+(prepare?360000:35000);startTimer(100);return true;
}
void RadioUpdater::timerCallback(){
    if(progressFile.existsAsFile()&&progressFile.getSize()<4096){auto v=juce::JSON::parse(progressFile);if(v.isObject()){double f=(double)v["fraction"];if(std::isfinite(f)&&f>=-1&&f<=1){progress=f;phase=juce::jlimit(0,5,(int)v["phase"]);}}}
    if(child&&child->isRunning()&&juce::Time::getMillisecondCounterHiRes()<deadline)return;
    stopTimer();bool timeout=child&&child->isRunning();if(timeout)child->kill();child.reset();
    auto result=timeout?failure("The update request timed out."):report.existsAsFile()&&report.getSize()<65536?juce::JSON::parse(report):failure("No valid update report was returned.");
    if(!result.isObject())result=failure("The update response could not be read.");clearFiles();auto completion=std::move(callback);if(completion)completion(result);
}
bool RadioUpdater::installAndRestart(const juce::var& prepared,juce::String& error){
    auto installed=appDirectory();auto marker=juce::JSON::parse(installed.getChildFile(".radioroom-install.json"));
    if(marker["product"].toString()!="audio.lakam.radioroom"||marker["installRoot"].toString()!=installed.getFullPathName()){error="Install RadioRoom using its package installer before using in-app installation.";return false;}
    auto cache=juce::File(juce::SystemStats::getEnvironmentVariable("LOCALAPPDATA",{})).getChildFile("RadioRoom/Updates");
    juce::File installer(prepared["installer"].toString()),package(prepared["packageRoot"].toString());
    if(!installer.existsAsFile()||!package.isDirectory()||!installer.isAChildOf(cache)||!package.isAChildOf(cache)){error="Verified update files are missing or outside the update cache.";return false;}
#if JUCE_WINDOWS
    juce::StringArray args{"-NoProfile","-NonInteractive","-ExecutionPolicy","Bypass","-File",installer.getFullPathName(),"-PackageRoot",package.getFullPathName(),"-InstallRoot",installed.getFullPathName(),"-Vst3Root",marker["vst3Root"].toString(),"-WaitForProcess",juce::String((int)GetCurrentProcessId()),"-Relaunch","-Report",cache.getParentDirectory().getChildFile("last-update-result.json").getFullPathName()};
    auto parameters="\""+args.joinIntoString("\" \"")+"\"";
    if((INT_PTR)ShellExecuteW(nullptr,L"open",powershell().getFullPathName().toWideCharPointer(),parameters.toWideCharPointer(),installer.getParentDirectory().getFullPathName().toWideCharPointer(),SW_HIDE)>32)return true;
#endif
    error="Could not start the installer; the current installation was retained.";return false;
}
