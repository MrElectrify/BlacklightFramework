#include <Base/Backbone.h>

#include <chrono>

using Blacklight::Base::Backbone;

void Backbone::Run()
{
	m_running = true;
	m_thread = std::thread([this]()
	{
		while (m_running)
		{
			for (auto& module : m_modules)
			{
				module.second->Update();
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
		}
	});
}

Backbone::~Backbone()
{
	m_running = false;
	m_thread.join();

	// destroy modules
	for (auto it = m_modules.begin(); it != m_modules.end(); it = m_modules.erase(it))
	{
		it->second->DestroyHooks();
	}
}