#ifndef BLACKLIGHT_BACKBONE_H_
#define BLACKLIGHT_BACKBONE_H_

/*
Backbone base
5/26/19 15:49
*/

#include <Base/Module.h>
#include <Utils/Singleton.h>

#include <forward_list>
#include <memory>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <vector>

namespace Blacklight
{
	namespace Base
	{
		/*
		The base backbone that handles all modules, and handles initialization and uninitialization of modules
		*/
		class Backbone : public Utils::Singleton<Backbone>
		{
		public:
			// Constructs and begins management of a module with the given arguments. Returns true on success
			template<typename T, typename... Args>
			bool CreateModule(Args&&... args)
			{
				auto pair = m_modules.try_emplace(T::TypeIndex(), std::make_shared<T>(std::forward<Args>(args)...));
				if (pair.second)
				{
					// create the hooks for the module
					pair.first->second->CreateHooks();
					return true;
				}
				else
				{
					return false;
				}
			}
			// Destructs the module, destroying hooks and calling the destructor. Returns true on success
			template<typename T>
			bool DestroyModule()
			{
				auto it = m_modules.find(T::TypeIndex());
				if (it == m_modules.end())
					return false;

				it->second->DestroyHooks();
				m_modules.erase(it);
			}
			// Returns a module by the given type. Returns nullptr if the module has not been created
			template<typename T>
			std::shared_ptr<T> GetModule()
			{
				auto it = m_modules.find(T::TypeIndex());
				if (it == m_modules.end())
					return nullptr;

				// we use reinterpret_cast here because we are casting a single-inheritance class up, avoid some RTTI stuff
				return std::reinterpret_pointer_cast<T>(it->second);
			}

			// Starts the backbone update thread
			void Run();

			~Backbone();
		private:
			bool m_running = false;
			std::unordered_map<Utils::Index_t, std::shared_ptr<Module>> m_modules;
			std::thread m_thread;
		};
	}
}

#endif