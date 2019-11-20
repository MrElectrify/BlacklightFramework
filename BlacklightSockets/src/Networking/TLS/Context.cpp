#include <Networking/TLS/Context.h>

#include <openssl/conf.h>

#include <stdexcept>

using Blacklight::Networking::TLS::Context;
using Blacklight::Networking::TLS::TLSMethodType;

int Context::s_instanceCount = 0;

Context::Context(TLSMethodType methodType)
{
	StaticInit();

	const Method_t* method;

	switch (methodType)
	{
	case TLSMethodType_v23:
		method = SSLv23_method();
		break;
	case TLSMethodType_v23CLIENT:
		method = SSLv23_client_method();
		break;
	case TLSMethodType_v23SERVER:
		method = SSLv23_server_method();
		break;
	default:
		throw std::runtime_error("Invalid Method Type");
	}

	m_handle = SSL_CTX_new(method);

	SSL_set_ecdh_auto(m_handle, 1);

	if (m_handle == nullptr)
		throw std::runtime_error("Unable to create SSL context");

	SSL_CTX_set_options(m_handle, SSL_OP_NO_SSLv3);
}

Context::Context(Context&& other)
{
	StaticInit();

	m_handle = other.m_handle;

	other.m_handle = nullptr;
}

void Context::SetOptions(int options)
{
	SSL_CTX_set_options(m_handle, options);
}

void Context::SetMinVersion(int version)
{
	SSL_CTX_set_min_proto_version(m_handle, version);
}

void Context::UseCertChainFile(const char* file)
{
	if (SSL_CTX_use_certificate_chain_file(m_handle, file) <= 0)
		throw std::runtime_error("Unable to use Cert Chain");
}

void Context::UsePrivKey(const char* file)
{
	if (SSL_CTX_use_PrivateKey_file(m_handle, file, SSL_FILETYPE_PEM) <= 0)
		throw std::runtime_error("Unable to use Private Key");
}

Context::NativeHandle_t* Context::GetNativeHandle()
{
	return m_handle;
}

Context::~Context()
{
	if (m_handle)
		SSL_CTX_free(m_handle);

	StaticUninit();
}

void Context::StaticInit()
{
	if (s_instanceCount == 0)
	{
		//SSL_load_error_strings();
		SSL_library_init();
		OpenSSL_add_ssl_algorithms();
	}

	++s_instanceCount;
}

void Context::StaticUninit()
{
	--s_instanceCount;

	if (s_instanceCount == 0)
	{
		//ERR_free_strings();
		EVP_cleanup();
	}
}