#include "ResourceManager.h"
#include "Environment/Environment.h"
#include "Engine.h"
#include "ConVar/ConVar.h"
#include "Timer/Timer.h"
#include "Thread/Thread.h"

#include <mutex>
#include <thread>

static std::mutex g_resourceManagerMutex;
static std::mutex g_resourceManagerLoadingWorkMutex;;

static void* _resourceLoaderThread(void* data);

class ResourceManagerLoaderThread {
public:
	TacoThread* thread;
	std::mutex loadingMutex;

	std::atomic<size_t> threadIndex;
	std::atomic<bool> running;
	std::vector<ResourceManager::LOADING_WORK>* loadingWork;
};

ConVar rm_numthreads("rm_numthreads", 3, "how many parallel resource loader threads are spawned once on startup (!), and subsequently used during runtime");
ConVar rm_warnings("rm_warnings", false);
ConVar rm_debug_async_delay("rm_debug_async_delay", 0.0f);
ConVar rm_interrupt_on_destroy("rm_interrupt_on_destroy", true);
ConVar debug_rm_("debug_rm", false);

ConVar* ResourceManager::debug_rm = &debug_rm_;

const char* ResourceManager::PATH_DEFAULT_IMAGES = "materials/";
const char* ResourceManager::PATH_DEFAULT_FONTS = "fonts/";
const char* ResourceManager::PATH_DEFAULT_SOUNDS = "sounds/";
const char* ResourceManager::PATH_DEFAULT_SHADERS = "shaders/";

ResourceManager::ResourceManager() {
	m_bNextLoadAsync = false;

	m_loadingWork.reserve(32);

	for (int i = 0; i < rm_numthreads.getInt(); i++) {
		ResourceManagerLoaderThread* loaderThread = new ResourceManagerLoaderThread();

		loaderThread->loadingMutex.lock();
		loaderThread->threadIndex = i;
		loaderThread->running = true;
		loaderThread->loadingWork = &m_loadingWork;
		loaderThread->thread = new TacoThread(_resourceLoaderThread, (void*)loaderThread);
		if (!loaderThread->thread->isReady()) {
			engine->showMessageError("ResourceManager Error", "Couldnt create thread");
			SAFE_DELETE(loaderThread->thread);
			SAFE_DELETE(loaderThread);
		}
		else {
			m_threads.push_back(loaderThread);
		}
	}
};

ResourceManager::~ResourceManager() {
	destroyResources();

	for (size_t i = 0; i < m_threads.size(); i++) {
		m_threads[i]->running = false;
	}
	for (size_t i = 0; i < m_threads.size(); i++) {
		const size_t threadIndex = m_threads[i]->threadIndex.load();

		bool hasLoadingWork = false;
		for (size_t w = 0; w < m_loadingWork.size(); w++) {
			if (m_loadingWork[w].threadIndex.atomic.load() == threadIndex) {
				hasLoadingWork = true;
				break;
			}
		}
		if (!hasLoadingWork)
			m_threads[i]->loadingMutex.unlock();
	}
	for (size_t i = 0; i < m_threads.size(); i++) {
		delete m_threads[i]->thread;
	}
	m_threads.clear();

	for (size_t i = 0; i < m_loadingWorkAsyncDestroy.size(); i++) {
		delete m_loadingWorkAsyncDestroy[i];
	}
	m_loadingWorkAsyncDestroy.clear();
}

void ResourceManager::update() {
	bool reLock = false;
	g_resourceManagerMutex.lock();
	{
		for (size_t i = 0; i < m_loadingWork.size(); i++) {
			if (m_loadingWork[i].done.atomic.load()) {
				if (debug_rm->getBool())
					debugLog("Resource Manager: Worker thread #%i finished.\n", i);
				Resource* rs = m_loadingWork[i].resource.atomic.load();
				const size_t threadIndex = m_loadingWork[i].threadIndex.atomic.load();

				g_resourceManagerLoadingWorkMutex.lock();
				{
					m_loadingWork.erase(m_loadingWork.begin() + i);
				}

				g_resourceManagerLoadingWorkMutex.unlock();

				i--;

				int numLoadingWorkForThreadIndex = 0;
				for (size_t w = 0; w < m_loadingWork.size(); w++) {
					if (m_loadingWork[w].threadIndex.atomic.load() == threadIndex) {
						numLoadingWorkForThreadIndex++;
					}
				}
				if (numLoadingWorkForThreadIndex < 1) {
					if (m_threads.size() > 0)
						m_threads[threadIndex]->loadingMutex.lock();
				}
				g_resourceManagerMutex.unlock();

				reLock = true;
				rs->load();
				break;
			}
		}
		if (reLock) {
			g_resourceManagerMutex.lock();
		}

		for (size_t i = 0; 0 < m_loadingWorkAsyncDestroy.size(); i++) {
			bool canBeDestroyed = true;
			for (size_t w = 0; w < m_loadingWork.size(); w++) {
				if (debug_rm->getBool())
					debugLog("Resource Manager: Waiting for async destroy of #%i ...\n", i);
				canBeDestroyed = false;
				break;
			}
			if (canBeDestroyed) {
				if (debug_rm->getBool())
					debugLog("Resource Manager: Async destroy of #%i\n", i);
				delete m_loadingWorkAsyncDestroy[i];
				m_loadingWorkAsyncDestroy.erase(m_loadingWorkAsyncDestroy.begin() + i);
				i--;
			}
		}
	}
	g_resourceManagerMutex.unlock();
}