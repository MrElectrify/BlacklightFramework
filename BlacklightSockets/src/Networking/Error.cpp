#include <Networking/Error.h>

#include <string.h>
#ifdef _WIN32
#include <Windows.h>
#undef GetMessage
#define strcpy(DEST, SOURCE) strcpy_s(DEST, SOURCE)
#elif __linux__
// use the xpg standard
#ifdef __cplusplus
extern "C"
{
#endif
	extern
		int __xpg_strerror_r(int errcode, char* buffer, size_t length);
#define strerror_r __xpg_strerror_r

#ifdef __cplusplus
}
#endif
#define strcpy(DEST, SOURCE) strncpy(DEST, SOURCE, sizeof(DEST) / sizeof(*DEST))
#endif

using Blacklight::Networking::_ErrorCode;
using Blacklight::Networking::ErrorCode;

ErrorCode::ErrorCode() : m_code(0)
{
	strcpy(m_message, "Code not set");
}

ErrorCode::ErrorCode(RawCode_t code) : m_code(code)
{
	// custom messages
	if (m_code >= ErrorCode_EOF)
	{
		switch (m_code)
		{
		case ErrorCode_EOF:
			strcpy(m_message, "End of file (Socket gracefully closed)");
			break;
		case ErrorCode_CONNECTED:
			strcpy(m_message, "Socket is already connected");
			break;
		case ErrorCode_DISCONNECTED:
			strcpy(m_message, "Socket is not connected");
			break;
		case ErrorCode_HANDSHAKE:
			strcpy(m_message, "Handshake failed");
			break;
		}
		return;
	}

#ifdef _WIN32
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr, m_code, NULL, m_message, sizeof(m_message) / sizeof(*m_message), nullptr);
#elif __linux__
	strerror_r(m_code, m_message, sizeof(m_message) / sizeof(*m_message));
#endif
}

ErrorCode::RawCode_t ErrorCode::GetRawCode() const
{
	return m_code;
}

const char* ErrorCode::GetMessage() const
{
	return m_message;
}

ErrorCode::operator ErrorCode::RawCode_t() const
{
	return m_code;
}