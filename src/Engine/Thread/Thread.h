#ifndef THREAD_H
#define THREAD_H

class ConVar;

class BaseThread {
public:
	virtual ~BaseThread() { ; }
	virtual bool isReady() = 0;
};

class TacoThread {
public:
	static ConVar* debug;

	typedef void* (*START_ROUTINE)(void*);

public:
	TacoThread(START_ROUTINE start_routine, void* arg);
	~TacoThread();

	bool isReady();

private:
	BaseThread* m_baseThread;
};

#endif // !THREAD_H
