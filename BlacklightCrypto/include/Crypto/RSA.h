#ifndef BLACKLIGHT_CRYPTO_RSA_H_
#define BLACKLIGHT_CRYPTO_RSA_H_

/*
RSA implementation
6/7/19 18:44
*/

#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN 1
#define NOMINMAX 1
#undef min
#undef max
#endif

#include <MPIR/mpirxx.h>
#include <PicoSHA2/PicoSHA2.h>

namespace Blacklight
{
	namespace Crypto
	{
		/* RSA provides a high-level interface to encryption and decryption using RSA, with
		 * OAEP padding as described by PKCS #1 v2.2
		 */
		template<size_t BITCOUNT>
		class RSA
		{
		public:
			constexpr static size_t BLOCKSIZE = BITCOUNT;
			constexpr static size_t OCTETCOUNT = BITCOUNT / 8;
			constexpr static size_t PRIMESIZE = BITCOUNT / 2;
			constexpr static size_t EXPSIZE = BITCOUNT / 8;
			constexpr static size_t MODSIZE = BITCOUNT / 8;	// theoretically it could be this big, so let's allow for it
			constexpr static size_t KEYSIZE = EXPSIZE + MODSIZE;

			// Private Key Structure. Contains P, Q, Phi, an Private Exponent D
			struct PrivateKey
			{
				mpz_class d;	// private exponent
				mpz_class e;	// public exponent
				mpz_class n;	// public modulus
				mpz_class p;	// prime 1
				mpz_class q;	// prime 2
			};
			// Public Key Structure. Contains Public Exponent and Public Modulus
			struct PublicKey
			{
				mpz_class e;	// public exponent
				mpz_class n;	// public modulus
			};

			// Constructor which initializes the random engine
			RSA() noexcept : m_rand(gmp_randinit_default) {}
			// Generates a private and public key
			std::pair<PrivateKey, PublicKey> GenerateKeys() noexcept
			{
				SeedRandom();

				auto res = std::make_pair<PrivateKey, PublicKey>({}, {});

				// generate random PRIMESIZE number, ensure it is at least PRIMESIZE
				res.first.p = m_rand.get_z_bits(PRIMESIZE) | (mpz_class(1) << (PRIMESIZE - 1));

				// make sure it is prime
				mpz_nextprime(res.first.p.get_mpz_t(), res.first.p.get_mpz_t());

				// generate another random PRIMESIZE number
				res.first.q = m_rand.get_z_bits(PRIMESIZE) | (mpz_class(1) << (PRIMESIZE - 1));
				// make sure it is also prime
				mpz_nextprime(res.first.q.get_mpz_t(), res.first.q.get_mpz_t());

				// phi, public modulus
				res.first.n = res.first.p * res.first.q;
				res.second.n = res.first.n;

				// coprime count
				mpz_class m = (res.first.p - 1) * (res.first.q - 1);

				// gcd
				mpz_class g;

				// set e, and make sure it is coprime to m
				res.first.e = 0x10001;
				res.second.e = 0x10001;

				while (mpz_gcd(g.get_mpz_t(), res.first.e.get_mpz_t(), m.get_mpz_t()), g != 1)
					res.first.e += 2;

				// perform inverse modulation to get the private exponent
				mpz_invert(res.first.d.get_mpz_t(), res.first.e.get_mpz_t(), m.get_mpz_t());

				return res;
			}

			// Encrypts the specified data with OAEP padding, with the specified Public Key
			std::vector<char> Encrypt(const PublicKey& pubKey, const std::vector<char>& raw)
			{
				SeedRandom();

				// total octet count - 2 * sizeof(SHA256 hash) - 2
				static constexpr size_t MAXSIZE = OCTETCOUNT - 2 * picosha2::k_digest_size - 2;
				
				// allocate space in the result for our output size
				std::vector<char> result(OCTETCOUNT * std::ceil(static_cast<float>(raw.size()) / MAXSIZE));

				size_t numBlocks = 0;
				size_t processed = 0;

				while (processed < raw.size())
				{
					std::vector<char> tmp(OCTETCOUNT);

					size_t offset = 0;

					// form the data block
					// get an accurate count of bytes to process so we can pad correctly
					size_t processedThisRound = (raw.size() - processed > MAXSIZE) ? MAXSIZE : raw.size() - processed;

					// standard dictates k - mLen - 2hLen - 2 zero octets
					size_t zeroLen = OCTETCOUNT - processedThisRound - 2 * picosha2::k_digest_size - 2;

					std::vector<char> DB(picosha2::k_digest_size + zeroLen + 1 + processedThisRound);

					// sha256 the raw data portion and store in DB
					picosha2::hash256(raw.begin() + processed, raw.begin() + processed + processedThisRound,
						DB.begin(), DB.begin() + picosha2::k_digest_size);

					offset += picosha2::k_digest_size + zeroLen;

					// 0x1 concatenated after the zeroes
					DB[offset] = 0x1;

					++offset;

					// concatenate the message M after 0x1
					memcpy(DB.data() + offset, raw.data() + processed, processedThisRound);

					processed += processedThisRound;

					// generate an hLen seed
					mpz_class seed = m_rand.get_z_bits(picosha2::k_digest_size * 8);

					std::vector<char> seedBuf(picosha2::k_digest_size);

					// copy random seed to buffer
					mpz_export(seedBuf.data(), nullptr, 1, 1, 1, 0, seed.get_mpz_t());

					auto dbMask = GenerateMask(seedBuf, OCTETCOUNT - picosha2::k_digest_size - 1);

					// XOR DB with the generated mask
					for (size_t i = 0; i < DB.size(); ++i)
						DB[i] ^= dbMask[i];

					auto seedMask = GenerateMask(DB, picosha2::k_digest_size);

					// XOR seed to get maskedSeed
					for (size_t i = 0; i < seedBuf.size(); ++i)
						seedBuf[i] ^= seedMask[i];

					// copy data and setup final encoded buffer
					// tmp[0] = 0x0;

					memcpy(tmp.data() + 1, seedBuf.data(), picosha2::k_digest_size);

					memcpy(tmp.data() + 1 + picosha2::k_digest_size, DB.data(), DB.size());

					// encrypt buffer

					mpz_class resultNum;

					mpz_import(resultNum.get_mpz_t(), OCTETCOUNT, 1, 1, 1, 0, tmp.data());

					mpz_class encrypted;

					mpz_powm(encrypted.get_mpz_t(), resultNum.get_mpz_t(), pubKey.e.get_mpz_t(), pubKey.n.get_mpz_t());

					// make sure we allow the proper number of zeroes before the payload, depending on the hash
					size_t leadingZeroes = OCTETCOUNT - std::ceil(mpz_sizeinbase(encrypted.get_mpz_t(), 16) / 2.f);

					mpz_export(result.data() + numBlocks * OCTETCOUNT + leadingZeroes, nullptr, 1, 1, 1, 0, encrypted.get_mpz_t());

					++numBlocks;
				}

				return result;
			}
			// Encrypts the specified string with OAEP padding, with the specified Public Key
			std::vector<char> Encrypt(const PublicKey& pubKey, const std::string& raw)
			{
				std::vector<char> data(raw.begin(), raw.end());
				return Encrypt(pubKey, data);
			}
			// Decrypts the specified data with OAEP padding, with the specified Private Key
			std::vector<char> Decrypt(const PrivateKey& privKey, const std::vector<char>& cipher)
			{
				std::vector<char> result;

				static_assert(OCTETCOUNT >= picosha2::k_digest_size * 2 + 2, "Bitcount is not high enough to support OAEP");

				// make sure it is a multiple of the octet count
				if (cipher.size() % OCTETCOUNT != 0)
#if !(BLACKLIGHT_NOTHROW) && !(BLACKLIGHT_NOSTRINGS)
					throw std::runtime_error("Invalid cipher text size");
#elif !(BLACKLIGHT_NOTHROW)
					throw 1;
#else
					return result;
#endif

				size_t blocksDecrypted = 0;
				while (blocksDecrypted < cipher.size() / OCTETCOUNT)
				{
					// create a temporary buffer unique for every payload
					std::vector<char> tmp(OCTETCOUNT);

					// convert the first block into an integer to decrypt
					mpz_class cipherNum;

					mpz_import(cipherNum.get_mpz_t(), OCTETCOUNT, 1, 1, 1, 0, cipher.data() + blocksDecrypted * OCTETCOUNT);

					// decrypt the data
					mpz_class decrypted;

					mpz_powm(decrypted.get_mpz_t(), cipherNum.get_mpz_t(), privKey.d.get_mpz_t(), privKey.n.get_mpz_t());
					
					// make sure we allow the proper number of zeroes before the payload, depending on the hash
					size_t leadingZeroes = OCTETCOUNT - std::ceil(mpz_sizeinbase(decrypted.get_mpz_t(), 16) / 2.f);

					// export the encoded block to our temporary buffer, starting past the initial 0
					mpz_export(tmp.data() + leadingZeroes, nullptr, 1, 1, 1, 0, decrypted.get_mpz_t());

					// extract masked seed
					std::vector<char> maskedSeed(tmp.begin() + 1, tmp.begin() + 1 + picosha2::k_digest_size);

					// extract masked DB
					std::vector<char> maskedDB(tmp.begin() + 1 + picosha2::k_digest_size, tmp.end());

					// generate seed mask of size hLen with the masked DB
					auto seedMask = GenerateMask(maskedDB, picosha2::k_digest_size);

					// xor the masked seed to get the original
					for (size_t i = 0; i < maskedSeed.size(); ++i)
						maskedSeed[i] ^= seedMask[i];

					auto dbMask = GenerateMask(maskedSeed, OCTETCOUNT - picosha2::k_digest_size - 1);

					// xor the masked db to get the original
					for (size_t i = 0; i < maskedDB.size(); ++i)
						maskedDB[i] ^= dbMask[i];

					// extract the hash for verification
					std::vector<char> hash(maskedDB.begin(), maskedDB.begin() + picosha2::k_digest_size);

					auto endPS = std::find(maskedDB.begin() + picosha2::k_digest_size, maskedDB.end(), 0x1);

					if (endPS == maskedDB.end())
#if !(BLACKLIGHT_NOTHROW) && !(BLACKLIGHT_NOSTRINGS)
						throw std::runtime_error("Could not find the end of the PS block");
#elif !(BLACKLIGHT_NOTHROW)
						throw 3;
#else
						return result;
#endif

					size_t messageLen = std::distance(endPS, maskedDB.end()) - 1;

					// create a temporary buffer for SHA-256
					std::vector<char> payloadHash(picosha2::k_digest_size);

					picosha2::hash256(endPS + 1, maskedDB.end(), payloadHash);

					if (payloadHash != hash)
#if !(BLACKLIGHT_NOTHROW) && !(BLACKLIGHT_NOSTRINGS)
						throw std::runtime_error("Data corrupted");
#elif !(BLACKLIGHT_NOTHROW)
						throw 4;
#else
						return result;
#endif

					// copy the message to the result buffer
					std::copy(endPS + 1, maskedDB.end(), std::back_inserter(result));

					++blocksDecrypted;
				}

				return result;
			}
		private:
			// MGF1 implementation
			std::vector<char> GenerateMask(const std::vector<char>& seed, const size_t length) const noexcept
			{
				std::vector<char> result(length);
				std::vector<char> temp(4);
				std::vector<char> tempSHA(picosha2::k_digest_size);

				std::copy(seed.begin(), seed.end(), std::back_inserter(temp));

				size_t offset = 0;
				size_t i = 0;

				while (offset < length)
				{
					temp[0] = (i >> 24);
					temp[1] = (i >> 16);
					temp[2] = (i >> 8);
					temp[3] = i;
					
					size_t remaining = length - offset;

					picosha2::hash256(temp, tempSHA);

					memcpy(result.data() + offset, tempSHA.data(), (remaining < picosha2::k_digest_size) ? remaining : picosha2::k_digest_size);

					offset += picosha2::k_digest_size;
					++i;
				}

				return result;
			}
			void SeedRandom()
			{
				// something high enough resolution
				auto timeSinceEpoch = std::chrono::system_clock::now().time_since_epoch();
				auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(timeSinceEpoch).count();

				m_rand.seed(ms);
			}

			gmp_randclass m_rand;
		};
	}
}

#endif