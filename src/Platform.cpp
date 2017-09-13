/*
* This file is part of Adblock Plus <https://adblockplus.org/>,
* Copyright (C) 2006-present eyeo GmbH
*
* Adblock Plus is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 3 as
* published by the Free Software Foundation.
*
* Adblock Plus is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with Adblock Plus.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <AdblockPlus/Platform.h>
#include <AdblockPlus/JsEngine.h>
#include <AdblockPlus/FilterEngine.h>
#include <AdblockPlus/DefaultLogSystem.h>
#include "DefaultTimer.h"
#include "DefaultWebRequest.h"
#include "DefaultFileSystem.h"
#include <stdexcept>

using namespace AdblockPlus;

namespace
{
  void DummyScheduler(const AdblockPlus::SchedulerTask& task)
  {
    std::thread(task).detach();
  }

  template<typename T>
  void ValidatePlatformCreationParameter(const std::unique_ptr<T>& param, const char* paramName)
  {
    if (!param)
      throw std::logic_error(paramName + std::string(" must not be nullptr"));
  }
}

#define ASSIGN_PLATFORM_PARAM(param) ValidatePlatformCreationParameter(param = std::move(creationParameters.param), #param)

Platform::Platform(CreationParameters&& creationParameters)
{
  ASSIGN_PLATFORM_PARAM(logSystem);
  ASSIGN_PLATFORM_PARAM(timer);
  ASSIGN_PLATFORM_PARAM(fileSystem);
  ASSIGN_PLATFORM_PARAM(webRequest);
}

Platform::~Platform()
{
}

void Platform::SetUpJsEngine(const AppInfo& appInfo, std::unique_ptr<IV8IsolateProvider> isolate)
{
  std::lock_guard<std::mutex> lock(modulesMutex);
  if (jsEngine)
    return;
  jsEngine = JsEngine::New(appInfo, *this, std::move(isolate));
}

JsEngine& Platform::GetJsEngine()
{
  SetUpJsEngine();
  return *jsEngine;
}

void Platform::CreateFilterEngineAsync(const FilterEngine::CreationParameters& parameters,
  const OnFilterEngineCreatedCallback& onCreated)
{
  std::shared_ptr<std::promise<FilterEnginePtr>> filterEnginePromise;
  {
    std::lock_guard<std::mutex> lock(modulesMutex);
    if (filterEngine.valid())
      return;
    filterEnginePromise = std::make_shared<std::promise<FilterEnginePtr>>();
    filterEngine = filterEnginePromise->get_future();
  }

  GetJsEngine(); // ensures that JsEngine is instantiated
  FilterEngine::CreateAsync(jsEngine, [this, onCreated, filterEnginePromise](const FilterEnginePtr& filterEngine)
  {
    filterEnginePromise->set_value(filterEngine);
    if (onCreated)
      onCreated(*filterEngine);
  }, parameters);
}

FilterEngine& Platform::GetFilterEngine()
{
  CreateFilterEngineAsync();
  return *std::shared_future<FilterEnginePtr>(filterEngine).get();
}

void Platform::WithTimer(const WithTimerCallback& callback)
{
  if (timer && callback)
    callback(*timer);
}

void Platform::WithFileSystem(const WithFileSystemCallback& callback)
{
  if (fileSystem && callback)
    callback(*fileSystem);
}

void Platform::WithWebRequest(const WithWebRequestCallback& callback)
{
  if (webRequest && callback)
    callback(*webRequest);
}

void Platform::WithLogSystem(const WithLogSystemCallback& callback)
{
  if (logSystem && callback)
    callback(*logSystem);
}

namespace
{
  class DefaultPlatform : public Platform
  {
  public:
    typedef std::shared_ptr<Scheduler> AsyncExecutorPtr;
    explicit DefaultPlatform(const AsyncExecutorPtr& asyncExecutor, CreationParameters&& creationParams)
      : Platform(std::move(creationParams)), asyncExecutor(asyncExecutor)
    {
    }
    ~DefaultPlatform();

    void WithTimer(const WithTimerCallback&) override;
    void WithFileSystem(const WithFileSystemCallback&) override;
    void WithWebRequest(const WithWebRequestCallback&) override;
    void WithLogSystem(const WithLogSystemCallback&) override;

  private:
    AsyncExecutorPtr asyncExecutor;
    std::recursive_mutex interfacesMutex;
  };

  DefaultPlatform::~DefaultPlatform()
  {
    asyncExecutor.reset();
    LogSystemPtr tmpLogSystem;
    TimerPtr tmpTimer;
    FileSystemPtr tmpFileSystem;
    WebRequestPtr tmpWebRequest;
    {
      std::lock_guard<std::recursive_mutex> lock(interfacesMutex);
      tmpLogSystem = std::move(logSystem);
      tmpTimer = std::move(timer);
      tmpFileSystem = std::move(fileSystem);
      tmpWebRequest = std::move(webRequest);
    }
  }

  void DefaultPlatform::WithTimer(const WithTimerCallback& callback)
  {
    std::lock_guard<std::recursive_mutex> lock(interfacesMutex);
    Platform::WithTimer(callback);
  }

  void DefaultPlatform::WithFileSystem(const WithFileSystemCallback& callback)
  {
    std::lock_guard<std::recursive_mutex> lock(interfacesMutex);
    Platform::WithFileSystem(callback);
  }

  void DefaultPlatform::WithWebRequest(const WithWebRequestCallback& callback)
  {
    std::lock_guard<std::recursive_mutex> lock(interfacesMutex);
    Platform::WithWebRequest(callback);
  }

  void DefaultPlatform::WithLogSystem(const WithLogSystemCallback& callback)
  {
    std::lock_guard<std::recursive_mutex> lock(interfacesMutex);
    Platform::WithLogSystem(callback);
  }
}

Scheduler DefaultPlatformBuilder::GetDefaultAsyncExecutor()
{
  if (!defaultScheduler)
  {
    asyncExecutor = std::make_shared<Scheduler>(::DummyScheduler);
    std::weak_ptr<Scheduler> weakExecutor = asyncExecutor;
    defaultScheduler = [weakExecutor](const SchedulerTask& task)
    {
      if (auto executor = weakExecutor.lock())
      {
        (*executor)(task);
      }
    };
  }
  return defaultScheduler;
}

void DefaultPlatformBuilder::CreateDefaultTimer()
{
  timer.reset(new DefaultTimer());
}

void DefaultPlatformBuilder::CreateDefaultFileSystem(const std::string& basePath)
{
  fileSystem.reset(new DefaultFileSystem(GetDefaultAsyncExecutor(), std::unique_ptr<DefaultFileSystemSync>(new DefaultFileSystemSync(basePath))));
}

void DefaultPlatformBuilder::CreateDefaultWebRequest(std::unique_ptr<IWebRequestSync> webRequest)
{
  if (!webRequest)
    webRequest.reset(new DefaultWebRequestSync());
  this->webRequest.reset(new DefaultWebRequest(GetDefaultAsyncExecutor(), std::move(webRequest)));
}

void DefaultPlatformBuilder::CreateDefaultLogSystem()
{
  logSystem.reset(new DefaultLogSystem());
}

std::unique_ptr<Platform> DefaultPlatformBuilder::CreatePlatform()
{
  if (!logSystem)
    CreateDefaultLogSystem();
  if (!timer)
    CreateDefaultTimer();
  if (!fileSystem)
    CreateDefaultFileSystem();
  if (!webRequest)
    CreateDefaultWebRequest();

  std::unique_ptr<Platform> platform(new DefaultPlatform(asyncExecutor, std::move(*this)));
  asyncExecutor.reset();
  return platform;
}
