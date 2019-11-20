#include <Networking/BLE/Context.h>

using Blacklight::Networking::BLE::Context;

void Context::UseKeyPair(const RSAHandle_t::PrivateKey& privKey, const RSAHandle_t::PublicKey& pubKey) noexcept
{
	m_priv = privKey;
	m_pub = pubKey;
}

void Context::PinKey(const RSAHandle_t::PublicKey& pinnedKey) noexcept
{
	m_pinnedKey = pinnedKey;
}

Context::RSAHandle_t& Context::RSAHandle() noexcept
{
	return m_rsa;
}

const Context::RSAHandle_t::PrivateKey& Context::GetPrivateKey() const noexcept
{
	return m_priv;
}

const Context::RSAHandle_t::PublicKey& Context::GetPublicKey() const noexcept
{
	return m_pub;
}

const Context::RSAHandle_t::PublicKey& Context::GetPinnedKey() const noexcept
{
	return m_pinnedKey;
}