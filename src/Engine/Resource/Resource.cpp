#include "Resource.h"
#include "Engine.h"
#include "Environment/Environment.h"

Resource::Resource(UString filePath) {
	m_sFilePath = filePath;

	if (filePath.length() > 0 && !env->fileExists(filePath)) {
		UString errorMessage = "File does not exist: ";
		errorMessage.append(filePath);
		debugLog("File %s does not exist!\n", filePath.toUtf8());
	}

	m_bReady = false;
	m_bAsyncReady = false;
	m_bInterrupted = false;
}

Resource::Resource() {
	m_bReady = false;
	m_bAsyncReady = false;
	m_bInterrupted = false;
}

void Resource::load() {
	init();
}

void Resource::loadAsync() {
	initAsync();
}

void Resource::reload() {
	release();
	loadAsync();
	load();
}

void Resource::release() {
	destroy();//lonely
	m_bReady = false;
	m_bAsyncReady = false;
}

void Resource::interruptLoad() {
	m_bInterrupted = true;
}