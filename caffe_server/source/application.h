#pragma once

#include <chrono>
#include <thread>
#include <atomic>

class Application {
	
public:
	static std::atomic<bool> _exit;

	Application() {
		
	}

	static void Exit() {
		_exit = true;
	}

	void Exec() {
		while (_exit == false)
		{
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
	}

};

std::atomic<bool> Application::_exit(false);