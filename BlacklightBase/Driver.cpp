#include <Base/Backbone.h>
#include <Base/Module.h>

#include <cstdio>

class TestModule : public Blacklight::Base::Module, public Blacklight::Utils::TemplateCounted<TestModule>
{
public:
	TestModule(int a) : m_a(a) { printf("TestModule::TestModule(): %d\n", a); }
	void CreateHooks() { printf("TestModule::CreateHooks()\n"); }
	void DestroyHooks() { printf("TestModule::DestroyHooks()\n"); }
	void Update() { printf("TestModule::Update()\n"); }
	void Test() { printf("TestModule::Test(): %d\n", m_a); }
	int GetA() const { return m_a; }
	~TestModule() { printf("TestModule::~TestModule()()\n"); }
private:
	int m_a;
};

class AnotherModule : public Blacklight::Base::Module, public Blacklight::Utils::TemplateCounted<AnotherModule>
{
public:
	AnotherModule(int a) : m_a(a) { printf("AnotherModule::AnotherModule(): %d\n", a); }
	void CreateHooks() { printf("AnotherModule::CreateHooks()\n"); }
	void DestroyHooks() { printf("AnotherModule::DestroyHooks()\n"); }
	void Update() { printf("AnotherModule::Update()\n"); }
	void Test() { printf("AnotherModule::Test(): %d\n", m_a); }
	int GetA() const { return m_a; }
	~AnotherModule() { printf("AnotherModule::~AnotherModule()()\n"); }
private:
	int m_a;
};

int main()
{
	Blacklight::Base::Backbone::GetInstance().CreateModule<TestModule>(5);
	Blacklight::Base::Backbone::GetInstance().CreateModule<AnotherModule>(6);

	Blacklight::Base::Backbone::GetInstance().GetModule<TestModule>()->Test();
	Blacklight::Base::Backbone::GetInstance().GetModule<AnotherModule>()->Test();

	Blacklight::Base::Backbone::GetInstance().Run();

	printf("TestModule::A: %d\n", Blacklight::Base::Backbone::GetInstance().GetModule<TestModule>()->GetA());
	printf("AnotherModule::A: %d\n", Blacklight::Base::Backbone::GetInstance().GetModule<AnotherModule>()->GetA());

	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	return 0;
}