#ifndef BLACKLIGHT_CRYPTO_AES_H_
#define BLACKLIGHT_CRYPTO_AES_H_

/*
AES implementation
6/9/19 10:53
*/

// STL
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// encryption library, actually implementing AES is not smart
#include <CryptoPP/aes.h>
#include <CryptoPP/filters.h>
#include <CryptoPP/gcm.h>
#include <CryptoPP/modes.h>
#include <CryptoPP/osrng.h>
#include <CryptoPP/secblock.h>

// fast rand
#include <Random/SSERand.h>

namespace Blacklight
{
	namespace Crypto
	{
		/* AES implements Rijndael in 128, 192, and 256-bit schemes, 
		 * using the GCM mode to ensure authenticity, with random IV generation
		 * which is calculated using he last sent data, and generated once in the beginning
		 */

		extern std::once_flag s_seeded;

		template<size_t BITCOUNT>
		class AES
		{
		public:
			constexpr static size_t TAGSIZE = 12;
			constexpr static size_t KEYSIZE = BITCOUNT / 8;
			constexpr static size_t IVSIZE = 128 / 8;

			static_assert((BITCOUNT == 128) || (BITCOUNT == 192) || (BITCOUNT == 256), "Bit count must be either 128, 192, or 256");
			// Constructor
			AES() noexcept 
			{
				// seed our random pool once
				std::call_once(s_seeded, []()
				{
					Random::SSERand::Seed(time(nullptr));
				});
			}

			// Move constructor
			AES(AES&& other)
				: m_decryption(std::move(other.m_decryption)), m_encryption(std::move(other.m_encryption)), m_previousIVs(std::move(other.m_previousIVs)) {}

			// Generates a key
			CryptoPP::SecByteBlock GenerateKey() const noexcept
			{
				CryptoPP::AutoSeededRandomPool rng;

				CryptoPP::SecByteBlock key(BITCOUNT / 8);
				rng.GenerateBlock(&key[0], key.size());

				return key;
			}
			// Generates an IV
			CryptoPP::SecByteBlock GenerateIV() const noexcept
			{
				CryptoPP::SecByteBlock iv(CryptoPP::AES::BLOCKSIZE);
				Random::SSERand::GenerateBlock(&iv[0]);
				
				// make sure we are generating a unique iv
				while (std::find(m_previousIVs.begin(), m_previousIVs.end(), iv) != m_previousIVs.end())
					Random::SSERand::GenerateBlock(&iv[0]);

				return iv;
			}
			// Generates a key and iv and returns them in a pair. @returns std::pair<key, iv>
			std::pair<CryptoPP::SecByteBlock, CryptoPP::SecByteBlock> GenerateKeyAndIV() const noexcept
			{
				return std::make_pair(GenerateKey(), GenerateIV());
			}

			// Encrypts the specified string with the key (ivs MUST be unique for each encryption pass), throws CryptoPP exceptions on failure
			std::vector<char> Encrypt(const CryptoPP::SecByteBlock& key, const CryptoPP::SecByteBlock& iv, const std::string& raw)
			{
				std::vector<char> res;

				if (std::find(m_previousIVs.begin(), m_previousIVs.end(), iv) != m_previousIVs.end())
#if !(BLACKLIGHT_NOTHROW) && !(BLACKLIGHT_NOSTRINGS)
					throw std::runtime_error("FATAL: reuse of IV");
#elif !(BLACKLIGHT_NOTHROW)
					throw 2;
#else
					return res;
#endif

				// initialize encryption
				m_encryption.SetKeyWithIV(key.data(), key.size(), iv.data(), iv.size());

				m_previousIVs.push_back(iv);

				std::copy(iv.begin(), iv.end(), std::back_inserter(res));

				CryptoPP::StringSource ss(raw, true, new CryptoPP::AuthenticatedEncryptionFilter(m_encryption,
					new CryptoPP::StringSinkTemplate<std::vector<char>>(res), false, TAGSIZE));

				return res;
			}
			// Encrypts the specified data with the key (ivs MUST be unique for each encryption pass), throws CryptoPP exceptions on failure
			std::vector<char> Encrypt(const CryptoPP::SecByteBlock& key, const CryptoPP::SecByteBlock& iv, const std::vector<char>& raw)
			{
				std::string data(raw.begin(), raw.end());

				return Encrypt(key, iv, data);
			}
			// Decrypts the specified data with the key and iv-pair, throws CryptoPP exceptions on failure
			std::vector<char> Decrypt(const CryptoPP::SecByteBlock& key, const std::vector<char>& raw)
			{
				std::vector<char> res;

				if (raw.size() < CryptoPP::AES::BLOCKSIZE + TAGSIZE)
#if !(BLACKLIGHT_NOTHROW) && !(BLACKLIGHT_NOSTRINGS)
					throw std::runtime_error("Size of decryption buffer is too small");
#elif !(BLACKLIGHT_NOTHROW)
					throw 1;
#else
					return res;
#endif

				CryptoPP::SecByteBlock iv(reinterpret_cast<const CryptoPP::byte*>(raw.data()), CryptoPP::AES::BLOCKSIZE);
				std::string data(raw.begin() + CryptoPP::AES::BLOCKSIZE, raw.end());

				// initialize decryption
				m_decryption.SetKeyWithIV(key.data(), key.size(), iv.data(), iv.size());

				// add to our list of previous to make sure we do not reuse an IV that was used for encryption
				if (std::find(m_previousIVs.begin(), m_previousIVs.end(), iv) == m_previousIVs.end())
					m_previousIVs.push_back(iv);

				CryptoPP::StringSource ss(data, true, new CryptoPP::AuthenticatedDecryptionFilter(m_decryption,
					new CryptoPP::StringSinkTemplate<std::vector<char>>(res), CryptoPP::AuthenticatedDecryptionFilter::DEFAULT_FLAGS, TAGSIZE));

				return res;
			}
			// Decrypts the specified data with the key and iv-pair, throws CryptoPP exceptions on failure
			std::vector<char> Decrypt(const CryptoPP::SecByteBlock& key, const std::string& raw)
			{
				std::vector<char> data(raw.begin(), raw.end());
				
				return Decrypt(key, data);
			}
		private:
			CryptoPP::GCM<CryptoPP::AES>::Decryption m_decryption;
			CryptoPP::GCM<CryptoPP::AES>::Encryption m_encryption;

			std::vector<CryptoPP::SecByteBlock> m_previousIVs;	// we need this because GCM (CTR) completely fails if we reuse an IV
		};
	}
}

#endif