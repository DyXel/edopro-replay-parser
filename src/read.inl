template<typename T>
constexpr auto read(uint8_t const*& ptr) -> T
{
	T value{};
	std::memcpy(&value, ptr, sizeof(T));
	ptr += sizeof(T);
	return value;
}

template<typename T>
constexpr auto read(uint8_t*& ptr) -> T
{
	return read<T>(const_cast<uint8_t const*&>(ptr));
}
