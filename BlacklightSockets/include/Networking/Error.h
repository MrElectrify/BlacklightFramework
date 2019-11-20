#ifndef BLACKLIGHT_NETWORKING_ERROR_H_
#define BLACKLIGHT_NETWORKING_ERROR_H_

/*
Defines network errors
4/18/19 23:47
*/

#ifdef _WIN32
#undef GetMessage
#endif

namespace Blacklight
{
	namespace Networking
	{
		enum _ErrorCode
		{
			// Operation completed successfully
			ErrorCode_SUCCESS = 0,
			// End of file, the socket closed gracefully
			ErrorCode_EOF = 0xb1ac1,
			// Already connected
			ErrorCode_CONNECTED,
			// Not connected
			ErrorCode_DISCONNECTED,
			// Handshake failed
			ErrorCode_HANDSHAKE
		};

		class ErrorCode
		{
		public:
			using RawCode_t = int;

			// constructs an empty ErrorCode
			ErrorCode();
			// constructs with a raw _ErrorCode
			ErrorCode(RawCode_t code);

			// get the raw error code
			RawCode_t GetRawCode() const;
			// get a message representing the error code
			const char* GetMessage() const;

			// implicit conversion to the raw error
			operator RawCode_t() const;
		private:
			RawCode_t m_code;
			char m_message[128];
		};
	}
}

#endif