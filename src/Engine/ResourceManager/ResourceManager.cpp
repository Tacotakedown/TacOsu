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

void ResourceManager::destroyResources() {
	while (m_vResources.size() > 0) {
		destroyResource(m_vResources[0]);
	}
	m_vResources.clear();
}

void ResourceManager::destroyResource(Resource* rs) {
	if (rs == NULL) {
		if (rm_warnings.getBool())
			debugLog("RESOURCE MANAGER Warning: destroyResource(NULL)!\n");
		return;
	}
	if (debug_rm->getBool())
		debugLog("ResourceManager: Destroying %s\n", rs->getName().toUtf8());
	g_resourceManagerMutex.lock();
	{
		bool isManagedResource = false;
		int managedResourceIndex = -1;
		for (size_t i = 0; i < m_vResources.size(); i++) {
			if (m_vResources[i] == rs) {
				isManagedResource = true;
				managedResourceIndex = i;
				break;
			}
		}
		for (size_t w = 0; w < m_loadingWork.size(); w++) {
			if (m_loadingWork[w].resource.atomic.load() == rs) {
				if (debug_rm->getBool())
					debugLog("Resource Manager: Scheduled async destroy of %s\n", rs->getName().toUtf8());
				if (rm_interrupt_on_destroy.getBool())
					rs->interruptLoad();
				m_loadingWorkAsyncDestroy.push_back(rs);
				if (isManagedResource)
					m_vResources.erase(m_vResources.begin() + managedResourceIndex);
				g_resourceManagerMutex.unlock();
				return;
			}
		}
		SAFE_DELETE(rs);
		if (isManagedResource)
			m_vResources.erase(m_vResources.begin() + managedResourceIndex);
	}
	g_resourceManagerMutex.unlock();
}

void ResourceManager::reloadResources() {
	for (size_t i = 0; i < m_vResources.size(); i++) {
		m_vResources[i]->reload();
	}
}

void ResourceManager::requestNextLoadAsync() {
	m_bNextLoadAsync = true;
}

void ResourceManager::requestNextLoadUnmanaged() {
	m_nextLoadUnmanagedStack.push(true);
}

Image* ResourceManager::loadImage(UString filePath, UString resourceName, bool mipmapped, bool keepInSystemMemory) {
	if (resourceName.length() > 0) {
		Resource* temp = checkIfExistsAndHandle(resourceName);
		if (temp != NULL)
			return dynamic_cast<Image*>(temp);
	}
	filePath.insert(0, PATH_DEFAULT_IMAGES);
	Image* img = engine->getGraphics()->createImage(filePath, mipmapped, keepInSystemMemory);
	img->setName(resourceName);

	loadResource(img, true);

	return img;
}

Image* ResourceManager::loadImageUnnamed(UString filePath, bool mipmapped, bool keepInSystemMemory) {
	filePath.insert(0, PATH_DEFAULT_IMAGES);
	Image* img = engine->getGraphics()->createImage(filePath, mipmapped, keepInSystemMemory);

	loadResource(img, true);

	return img;
}

Image* ResourceManager::loadImageAbs(UString absoluteFilePath, UString resourceName, bool mipmapped, bool keepInSystemMemory) {
	if (resourceName.length() > 0) {
		Resource* temp = checkIfExistsAndHandle(resourceName);
		if (temp != NULL)
			return dynamic_cast<Image*>(temp);
	}
	Image* img = engine->getGraphics()->createImage(absoluteFilePath, mipmapped, keepInSystemMemory);
	img->setName(resourceName);
	loadResource(img, true);

	return img;
}

Image* ResourceManager::loadImageUnnamed(UString absoluteFilePath, bool mipmapped, bool keepInSystemMemory) {
	Image* img = engine->getGraphics()->createImage(absoluteFilePath, mipmapped, keepInSystemMemory);

	loadResource(img, true);

	return img;
}

Image* ResourceManager::loadImageAbs(UString absoluteFilepath, UString resourceName, bool mipmapped, bool keepInSystemMemory) {
	if (resourceName.length() > 0) {
		Resource* temp = checkIfExistsAndHandle(resourceName);
		if (temp != NULL)
			return dynamic_cast<Image*>(temp);
	}

	Image* img = engine->getGraphics()->createImage(absoluteFilepath, mipmapped, keepInSystemMemory);
	img->setName(resourceName);

	loadResource(img, true);

	return img;
}

Image* ResourceManager::loadImageAbsUnnamed(UString absoluteFilepath, bool mipmapped, bool keepInSystemMemory) {
	Image* img = engine->getGraphics()->createImage(absoluteFilepath, mipmapped, keepInSystemMemory);

	loadResource(img, true);

	return img;
}


Image* ResourceManager::createImage(unsigned int width, unsigned int height, bool mipmapped, bool keepInSystemMemory) {
	if (width > 8192 || height > 8192) {
		engine->showMessageError("Resource Manager Error", UString::format("Invalid parameters in createImage(%i, %i, %i)!\n", width, height, (int)mipmapped));
		return NULL;
	}
	Image* img = engine->getGraphics()->createImage(width, height, mipmapped, keepInSystemMemory);

	loadResource(img, false);

	return img;
}

TacoFont* ResourceManager::loadFont(UString filePath, UString resourceName, int fontSize, bool antialiasing, int fontDPI) {
	if (resourceName.length() > 0) {
		Resource* temp = checkIfExistsAndHandle(resourceName);
		if (temp != NULL)
			return dynamic_cast<TacoFont*>(temp);
	}
	filePath.insert(0, PATH_DEFAULT_FONTS);
	TacoFont* fnt = new TacoFont(filePath, fontSize, antialiasing, fontDPI);
	fnt->setName(resourceName);

	loadResource(fnt, true);

	return fnt;
}

TacoFont* ResourceManager::loadFont(UString filePath, UString resourceName, std::vector<wchar_t> characters, int fontSize, bool antialiasing, int fontDPI) {
	if (resourceName.length() > 0) {
		Resource* temp = checkIfExistsAndHandle(resourceName);
		if (temp != NULL)
			return dynamic_cast<TacoFont*>(temp);
	}

	filePath.insert(0, PATH_DEFAULT_FONTS);
	TacoFont* fnt = new TacoFont(filePath, characters, fontSize, antialiasing, fontDPI);
	fnt->setName(resourceName);

	loadResource(fnt, true);

	return fnt;
}

Sound* ResourceManager::loadSound(UString filePath, UString resourceName, bool stream, bool threeD, bool loop, bool prescan) {
	if (resourceName.length() > 0) {
		Resource* temp = checkIfExistsAndHandle(resourceName);
		if (temp != NULL)
			return dynamic_cast<Sound*>(temp);
	}

	filePath.insert(0, PATH_DEFAULT_SOUNDS);
	Sound* snd = new Sound(filePath, stream, threeD, loop, prescan);
	snd->setName(resourceName);

	loadResource(snd, true);

	return snd;
}

Sound* ResourceManager::loadSoundAbs(UString filePath, UString resourceName, bool stream, bool threeD, bool loop, bool prescan) {
	if (resourceName.length() > 0) {
		Resource* temp = checkIfExistsAndHandle(resourceName);
		if (temp != NULL)
			return dynamic_cast<Sound*>(temp);
	}

	Sound* snd = new Sound(filePath, stream, threeD, loop, prescan);
	snd->setName(resourceName);

	loadResource(snd, true);

	return snd;
}

Shader* ResourceManager::loadShader(UString vertexShaderFilePath, UString fragmentShaderFilePath, UString resourceName) {

	if (resourceName.length() > 0) {
		Resource* temp = checkIfExistsAndHandle(resourceName);
		if (temp != NULL)
			return dynamic_cast<Shader*>(temp);
	}

	vertexShaderFilePath.insert(0, PATH_DEFAULT_SHADERS);
	fragmentShaderFilePath.insert(0, PATH_DEFAULT_SHADERS);
	Shader* shader = engine->getGraphics()->createShaderFromFile(vertexShaderFilePath, fragmentShaderFilePath);
	shader->setName(resourceName);

	loadResource(shader, true);

	return shader;
}

Shader* ResourceManager::loadShader(UString vertexShaderFilePath, UString fragmentShaderFilePath) {
	vertexShaderFilePath.insert(0, PATH_DEFAULT_SHADERS);
	fragmentShaderFilePath.insert(0, PATH_DEFAULT_SHADERS);
	Shader* shader = engine->getGraphics()->createShaderFromFile(vertexShaderFilePath, fragmentShaderFilePath);

	loadResource(shader, true);

	return shader;
}

Shader* ResourceManager::createShader(UString vertexShader, UString fragmentShader, UString resourceName) {
	if (resourceName.length() > 0) {
		Resource* temp = checkIfExistsAndHandle(resourceName);
		if (temp != NULL)
			return dynamic_cast<Shader*>(temp);
	}

	Shader* shader = engine->getGraphics()->createShaderFromSource(vertexShader, fragmentShader);
	shader->setName(resourceName);

	loadResource(shader, true);

	return shader;
}

Shader* ResourceManager::createShader(UString vertexShader, UString fragmentShader) {
	Shader* shader = engine->getGraphics()->createShaderFromSource(vertexShader, fragmentShader);

	loadResource(shader, true);

	return shader;
}

RenderTarget* ResourceManager::createRenderTarget(int x, int y, int width, int height, Graphics::MULTISAMPLE_TYPE multiSampleType) {
	RenderTarget* rt = engine->getGraphics()->createRenderTarget(x, y, width, height, multiSampleType);
	rt->setName(UString::format("_RT_%ix%i", width, height));

	loadResource(rt, true);

	return rt;
}

RenderTarget* ResourceManager::createRenderTarget(int width, int height, Graphics::MULTISAMPLE_TYPE multiSampleType) {
	return createRenderTarget(0, 0, width, height, multiSampleType);
}

TextureAtlas* ResourceManager::createTextureAtlas(int width, int height) {
	TextureAtlas* ta = new TextureAtlas(width, height);
	ta->setName(UString::format("_TA_%ix%i", width, height));

	loadResource(ta, false);

	return ta;
}

VertexArrayObject* ResourceManager::createVertexArrayObject(Graphics::PRIMITIVE primitive, Graphics::USAGE_TYPE usage, bool keepInSystemMemory) {
	VertexArrayObject* vao = engine->getGraphics()->createVertexArrayObject(primitive, usage, keepInSystemMemory);

	loadResource(vao, false);

	return vao;
}

Image* ResourceManager::getImage(UString resourceName) const {
	for (size_t i = 0; i < m_vResources.size(); i++) {
		if (m_vResources[i]->getName() == resourceName)
			return dynamic_cast<Image*>(m_vResources[i]);
	}
	doesntExistWarning(resourceName);
	return NULL;
}

TacoFont* ResourceManager::getFont(UString resourceName) const {
	for (size_t i = 0; i < m_vResources.size(); i++) {
		if (m_vResources[i]->getName() == resourceName)
			return dynamic_cast<TacoFont*>(m_vResources[i]);
	}
	doesntExistWarning(resourceName);
	return NULL;
}

Sound* ResourceManager::getSound(UString resourceName) const {
	for (size_t i = 0; i < m_vResources.size(); i++) {
		if (m_vResources[i]->getName() == resourceName)
			return dynamic_cast<Sound*>(m_vResources[i]);
	}
	doesntExistWarning(resourceName);
	return NULL;
}

Shader* ResourceManager::getShader(UString resourceName) const {
	for (size_t i = 0; i < m_vResources.size(); i++) {
		if (m_vResources[i]->getName() == resourceName)
			return dynamic_cast<Shader*>(m_vResources[i]);
	}
	doesntExistWarning(resourceName);
	return NULL;
}

bool ResourceManager::isLoading() const {
	return(m_loadingWork.size() > 0);
}

bool ResourceManager::isLoadingResource(Resource* rs) const {
	for (size_t i = 0; i < m_loadingWork.size(); i++) {
		if (m_loadingWork[i].resource.atomic.load() == rs)
			return true;
	}
	return false;
}

void ResourceManager::loadResource(Resource* res, bool load) {
	if (m_nextLoadUnmanagedStack.size() < 1 || !m_nextLoadUnmanagedStack.top())
		m_vResources.push_back(res);

	const bool isNextLoadAsync = m_bNextLoadAsync;

	resetFlags();

	if (!load) return;

	if (!isNextLoadAsync) {
		res->loadAsync();
		res->load();
	}
	else {
		if (rm_numthreads.getInt() > 0) {
			g_resourceManagerMutex.lock();
			{
				static size_t threadIndexCounter = 0;
				const size_t threadIndex = threadIndexCounter;

				LOADING_WORK work;

				work.resource = MobileAtomicResource(res);
				work.threadIndex = MobileAtomicSizeT(threadIndex);
				work.done = MobileAtomicBool(false);

				threadIndexCounter = (threadIndexCounter + 1) % (std::min(m_threads.size(), (size_t)std::max(rm_numthreads.getInt(), 1)));

				g_resourceManagerLoadingWorkMutex.lock();
				{
					m_loadingWork.push_back(work);

					int numLoadingWorkForThreadIndex = 0;
					for (size_t i = 0; i < m_loadingWork.size(); i++) {
						if (m_loadingWork[i].threadIndex.atomic.load() == threadIndex)
							numLoadingWorkForThreadIndex++;
					}
					if (numLoadingWorkForThreadIndex == 1) {
						if (m_threads.size() > 0)
							m_threads[threadIndex]->loadingMutex.unlock();
					}
				}
				g_resourceManagerLoadingWorkMutex.unlock();
			}
			g_resourceManagerMutex.unlock();
		}
		else {
			res->loadAsync();
			res->load();
		}
	}
}

void ResourceManager::doesntExistWarning(UString resourceName) const {
	if (rm_warnings.getBool()) {
		UString errormsg = "Resource \"";
		errormsg.append(resourceName);
		errormsg.append("\" does not exist!");
		engine->showMessageWarning("RESOURCE MANAGER: ", errormsg);
	}
}

Resource* ResourceManager::checkIfExistsAndHandle(UString resourceName) {
	for (size_t i = 0; i < m_vResources.size(); i++) {
		if (m_vResources[i]->getName() == resourceName) {
			if (rm_warnings.getBool())
				debugLog("RESOURCE MANAGER: Resource \"%s\" already loaded!\n", resourceName.toUtf8());
			resetFlags();

			return m_vResources[i];
		}
	}
	return NULL;
}

void ResourceManager::resetFlags() {
	if (m_nextLoadUnmanagedStack.size() > 0)
		m_nextLoadUnmanagedStack.pop();

	m_bNextLoadAsync = false;
}

static void* _resourceLoaderThread(void* data) {
	ResourceManagerLoaderThread* self = (ResourceManagerLoaderThread*)data;

	const size_t threadIndex = self->threadIndex.load();

	while (self->running.load()) {
		self->loadingMutex.lock();
		self->loadingMutex.unlock();

		Resource* resourceToLoad = NULL;
		g_resourceManagerLoadingWorkMutex.lock();
		{
			for (size_t i = 0; i < self->loadingWork->size(); i++) {
				if ((*self->loadingWork)[i].threadIndex.atomic.load() == threadIndex && !(*self->loadingWork)[i].done.atomic.load()) {
					resourceToLoad = (*self->loadingWork)[i].resource.atomic.load();
					break;
				}
			}
		}
		g_resourceManagerLoadingWorkMutex.unlock();

		if (resourceToLoad != NULL) {
			if (rm_debug_async_delay.getFloat() > 0.0f)
				env->sleep(rm_debug_async_delay.getFloat() * 1000 * 1000);

			resourceToLoad->loadAsync();

			g_resourceManagerLoadingWorkMutex.lock();
			{
				for (size_t i = 0; i < self->loadingWork->size(); i++) {
					if ((*self->loadingWork)[i].threadIndex.atomic.load() == threadIndex && (*self->loadingWork)[i].resource.atomic.load() == resourceToLoad) {
						(*self->loadingWork)[i].done = ResourceManager::MobileAtomicBool(true);
						break;
					}
				}
			}
			g_resourceManagerLoadingWorkMutex.unlock();
		}
		else
			env->sleep(1000);
	}
	return NULL;
}