#ifndef BLACKLIGHT_NETWORKING_BLE_CONTEXT_H_
#define BLACKLIGHT_NETWORKING_BLE_CONTEXT_H_

/*
BLE Context
6/10/19 12:51
*/

#include <Crypto/RSA.h>

namespace Blacklight
{
	namespace Networking
	{
		namespace BLE
		{
			/*
			 *	BLE Context holds necessary RSA information for key negotiation
			 */
			class Context
			{
			public:
				using RSAHandle_t = Crypto::RSA<4096>;
				
				// Sets the socket to use a keyPair. If not set, they will be generated randomly
				void UseKeyPair(const RSAHandle_t::PrivateKey& privateKey, const RSAHandle_t::PublicKey& publicKey) noexcept;
				// Sets the socket to pin a public key in client mode
				void PinKey(const RSAHandle_t::PublicKey& pinnedKey) noexcept;

				// Returns the native RSA handle
				RSAHandle_t& RSAHandle() noexcept;
				// Return Private Key
				const RSAHandle_t::PrivateKey& GetPrivateKey() const noexcept;
				// Return Public Key
				const RSAHandle_t::PublicKey& GetPublicKey() const noexcept;
				// Return Pinned Key
				const RSAHandle_t::PublicKey& GetPinnedKey() const noexcept;
			private:
				RSAHandle_t m_rsa;

				RSAHandle_t::PrivateKey m_priv;
				RSAHandle_t::PublicKey m_pub;

				RSAHandle_t::PublicKey m_pinnedKey;
			};
		}
	}
}

#endif