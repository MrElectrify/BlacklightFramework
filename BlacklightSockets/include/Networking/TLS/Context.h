#ifndef BLACKLIGHT_SSL_CONTEXT_H_
#define BLACKLIGHT_SSL_CONTEXT_H_

#include <openssl/ssl.h>

namespace Blacklight
{
	namespace Networking
	{
		namespace TLS
		{
			// only supports v23 right now
			enum TLSMethodType
			{
				TLSMethodType_v23,
				TLSMethodType_v23SERVER,
				TLSMethodType_v23CLIENT
			};

			/*
			Manages the global SSL context for TLS
			*/
			class Context
			{
			public:
				using Method_t = SSL_METHOD;
				using NativeHandle_t = SSL_CTX;

				// construct a context by a method type
				Context(TLSMethodType methodType);
				// move construct
				Context(Context&& other);

				Context(const Context& other) = delete;
				Context& operator=(const Context& other) = delete;

				// sets an SSL option
				void SetOptions(int options);

				// sets the minimum version of SSL required
				void SetMinVersion(int version);

				// use the specified certificate chain file
				void UseCertChainFile(const char* file);

				// use the specified private key
				void UsePrivKey(const char* file);

				NativeHandle_t* GetNativeHandle();

				~Context();
			private:
				static int s_instanceCount;

				NativeHandle_t* m_handle;

				static void StaticInit();
				static void StaticUninit();
			};
		}
	}
}

#endif