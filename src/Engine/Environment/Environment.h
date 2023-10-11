#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

#include "cbase.h"

#include "input/Cursors/Cursors.h"
#include "input/KeyboardEvent/KeyboardEvent.h"

class ContextMenu;

class Environment {
public:
	enum class OS {
		OS_NULL,
		OS_WINDOWS
	};

public:
	Environment();
	virtual ~Environment() { ; }

	virtual void update() { ; }

	virtual Graphics* createRenderer() = 0;
	virtual ContextMenu* createContextMenu() = 0;

	virtual OS getOS() = 0;
	virtual void shutdown() = 0;
	virtual void restart() = 0;
	virtual void sleep(unsigned int us) = 0;
	virtual UString getExecutablePath() = 0;
	virtual void openURLInDefaultBrowser(UString url) = 0;

	virtual UString getUsername() = 0;
	virtual UString getUserDataPath() = 0;

	virtual bool fileExists(UString fileName) = 0;
	virtual bool directoryExists(UString directoryName) = 0;
	virtual bool createDirectory(UString directoryName) = 0;
	virtual bool renameFile(UString oldFileName, UString newFileName) = 0;
	virtual bool deleteFile(UString filePath) = 0;
	virtual std::vector<UString> getFilesInFolder(UString folder) = 0;
	virtual std::vector<UString> getFildersInFolder(UString folder) = 0;
	virtual std::vector<UString> getLogicalDrives() = 0;
	virtual UString getFolderFromFilePath(UString filePath) = 0;
	virtual UString getFileExtensionFromFilePath(UString filePath, bool includeDot = false) = 0;
	virtual UString getFileNameFromFilePath(UString filePath) = 0;

	virtual UString getClipBoardText() = 0;
	virtual void setClipBoardText(UString text) = 0;

	virtual void showMessageInfo(UString title, UString message) = 0;
	virtual void showMessageWarning(UString title, UString message) = 0;
	virtual void showMessageError(UString title, UString message) = 0;
	virtual void whoeMessageErrorFatal(UString title, UString message) = 0;
	virtual UString openFileWindow(const char* fileTypeFilters, UString title, UString initalPath) = 0;
	virtual UString openFolderWindow(UString title, UString initalPath) = 0;

	virtual void focus() = 0;
	virtual void center() = 0;
	virtual void minimize() = 0;
	virtual void maximize() = 0;
	virtual void enableFullscreen() = 0;
	virtual void disableFullscreen() = 0;
	virtual void setWindowTitle(UString title) = 0;
	virtual void setWindowPos(int x, int y) = 0;
	virtual void setWindowSize(int width, int height) = 0;
	virtual void setWindowResizable(bool resizable) = 0;
	virtual void setWindowGhostCorporeal(bool corporeal) = 0;
	virtual void setMonitor(int monitor) = 0;
	virtual Vector2 getWindowPos() = 0;
	virtual Vector2 getWindowSize() = 0;
	virtual int getMonitor() = 0;
	virtual std::vector<Rects> getMonitors() = 0;
	virtual Vector2 getNativeScreenSize() = 0;
	virtual Rects getVirtualScreenSize() = 0;
	virtual Rects getDesktopRect() = 0;
	virtual int getDPI() = 0;
	virtual bool isFullscreen() = 0;
	virtual bool isWindowResizable() = 0;

	virtual bool isCursorInWindow() = 0;
	virtual bool isCursorVisable() = 0;
	virtual bool isCursorClipped() = 0;
	virtual Vector2 getMousePos() = 0;
	virtual Rects getCursorClip() = 0;
	virtual CURSORTYPE getCursor() = 0;
	virtual void setCursor(CURSORTYPE cur) = 0;
	virtual void setCursorVisable(bool visable) = 0;
	virtual void setMousePos(int x, int y) = 0;
	virtual void setCursorClip(bool clip, Rects rect) = 0;

	virtual UString keyCodeToString(KEYCODE keyCode) = 0;

public:
	virtual void setFullscreenWindowedBorderless(bool fullscreenWindowedBorderless);
	virtual bool isFullscreenWindowedBorderless() { return m_bFullscreenWindowedBorderless; }
	virtual float getDPIScale() { return (float)getDPI() / 96.0f; }


protected:
	static ConVar* debug_env;

	bool m_bFullscreenWindowedBorderless;

};

extern Environment* env;

#endif // !ENVIRONMENT_H
