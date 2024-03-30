//#include <memory>
//#include <utility>
//
//struct package {
//	using source_type_t = uint16_t;
//	using protocol_version_t = uint16_t;
//	using data_length_t = uint16_t; // no need for big number here. Used to count items here
//
//	static constexpr size_t package_items_length_max = 1 << 6; // Max size of data stored in one package
//	static constexpr size_t package_items_key_string_length_max = 1 << 4; // used in items to identify them later
//	static constexpr size_t packages_max_items = 1 << 4; // Max information (data) combined
//
//	/* Any data + type */
//	struct data {
//		/* Types allowed / implemented */
//		enum class data_type {
//			INVALID, FLOAT, DOUBLE, UNSIGNED, INTEGER
//		};
//
//		/* Identify this with a key */
//		char m_key[package_items_key_string_length_max];
//
//		/* Allow store of some common types */
//		union _ {
//			const float m_float;
//			const double m_double;
//			const uint64_t m_u64;
//			const int64_t m_i64;
//
//			/* hard checks */
//			static_assert(sizeof(float) == 4, "Float is not 4 bytes in this implementation, things may break");
//			static_assert(sizeof(double) == 8, "Double is not 8 bytes in this implementation, things may break");
//			static_assert(sizeof(uint64_t) == 8, "UINT64 is not 8 bytes in this implementation, things may break");
//			static_assert(sizeof(int64_t) == 8, "INT64 is not 8 bytes in this implementation, things may break");
//
//			_() : m_float(0.0f) {} // default
//
//			/* Allow construct from any of the types */
//			_(float f)    : m_float(f) {}
//			_(double f)   : m_double(f) {}
//			_(uint64_t f) : m_u64(f) {}
//			_(int64_t f)  : m_i64(f) {}
//		} m_data;
//
//		/* Know what type is being stored */
//		data_type m_type = data_type::INVALID;
//
//		data() = default;
//
//		/* Automatic match */
//		data(const char* k, float f)    : m_data({ f }), m_type(data_type::FLOAT)		{ memcpy(m_key, k, std::min(sizeof(m_key) - 1, strlen(k) + 1)); m_key[sizeof(m_key) - 1] = '\0'; }
//		data(const char* k, double d)   : m_data({ d }), m_type(data_type::DOUBLE)		{ memcpy(m_key, k, std::min(sizeof(m_key) - 1, strlen(k) + 1)); m_key[sizeof(m_key) - 1] = '\0'; }
//		data(const char* k, uint64_t u) : m_data({ u }), m_type(data_type::UNSIGNED)	{ memcpy(m_key, k, std::min(sizeof(m_key) - 1, strlen(k) + 1)); m_key[sizeof(m_key) - 1] = '\0'; }
//		data(const char* k, int64_t i)  : m_data({ i }), m_type(data_type::INTEGER)		{ memcpy(m_key, k, std::min(sizeof(m_key) - 1, strlen(k) + 1)); m_key[sizeof(m_key) - 1] = '\0'; }
//
//		inline auto get_type() const		{ return m_type; }
//		inline auto get_key() const			{ return m_key; }
//
//		inline auto get_float() const		{ if (m_type == data_type::FLOAT) return m_data.m_float;  return 0.0f; }
//		inline auto get_double() const		{ if (m_type == data_type::FLOAT) return m_data.m_double; return 0.0; }
//		inline auto get_int() const			{ if (m_type == data_type::FLOAT) return m_data.m_i64;    return 0LL; }
//		inline auto get_unsigned() const	{ if (m_type == data_type::FLOAT) return m_data.m_u64;    return 0ULL; }
//	};
//
//	enum class source_type : source_type_t {
//		UNKNOWN = 0,
//		BMP180_SENSOR,	/* Atmosphere pressure sensor */
//		DHT22_SENSOR,	/* Temperature and Humidity sensor */
//		MHZ19B_SENSOR,	/* CO2 sensor based in infra-red */
//		CCS811_SENSOR,	/* Air quality sensor */
//		LDR_MOD,		/* Simple light sensor */
//		SDS011_SENSOR,	/* Particle sensor */
//		KY038_SENSOR,	/* Noise/sound sensor (microphone) */
//		BMI160_SENSOR,	/* Gyro 6 axis sensor */
//		BATTERY_SENSOR, /* Own battery reporting sensor */
//
//		CUSTOM_SENSOR = std::numeric_limits<source_type_t>::max() /* Uncategorized sensor, user-made */
//	};
//
//	/* Related to m_version, it's the protocol version. */
//	enum class protocol_version : protocol_version_t {
//		UNKNOWN = 0,
//		V0_0_1 /* Develop phase, current */
//	};
//
//	static constexpr protocol_version current_version = protocol_version::V0_0_1;
//
//
//	/* Later write/read, use default */
//	package() = default;
//
//	/* Automatic reading from function calling. */
//	package(void(*read_fcn)(void*, size_t)) {
//		from_function(read_fcn);
//	}
//
//	/* Build from source type but no values, for writing */
//	package(source_type kind, void(*write_fcn)(const void*, size_t))
//		: m_type(static_cast<source_type_t>(kind)), m_data_length(0), m_array_of_data({})
//	{
//		to_function(write_fcn);
//	}
//
//	/* Build from source type and values */
//	template<typename... _data> package(source_type kind, _data... items)
//		: m_type(static_cast<source_type_t>(kind)), m_data_length(sizeof...(items)),
//		  m_array_of_data(m_data_length <= packages_max_items ? std::unique_ptr<data[]>(new data[m_data_length]{ items... }) : std::unique_ptr<data[]>{})
//	{
//		if (m_data_length > packages_max_items) throw std::invalid_argument("Too many data items!");
//	}
//	/* Build from source type and values, for writing */
//	template<typename... _data> package(source_type kind, void(*write_fcn)(const void*, size_t), _data... items)
//		: m_type(static_cast<source_type_t>(kind)), m_data_length(sizeof...(items)), 
//		  m_array_of_data(m_data_length <= packages_max_items ? std::unique_ptr<data[]>(new data[m_data_length]{ items... }) : std::unique_ptr<data[]>{})
//	{
//		if (m_data_length > packages_max_items) throw std::invalid_argument("Too many data items!");
//		to_function(write_fcn);
//	}
//
//	void from_function(void(*read_fcn)(void*, size_t)) {
//		/* Expects in order of data, then m_data_length telling the package items size */
//		read_fcn(&m_version, sizeof(m_version));
//		if (m_version != static_cast<protocol_version_t>(current_version)) throw std::invalid_argument("Version mismatch");
//
//		read_fcn(&m_type, sizeof(m_type));
//
//		read_fcn(&m_data_length, sizeof(m_data_length));
//		if (m_data_length > packages_max_items) throw std::invalid_argument("Too many items!");
//		if (m_data_length == 0) return;
//
//		m_array_of_data = std::unique_ptr<data[]>(new data[m_data_length]);
//
//		for (data_length_t i = 0; i < m_data_length; ++i) {
//			read_fcn(&m_array_of_data[i], sizeof(data));
//		}
//	}
//
//	void to_function(void(*write_to)(const void*, size_t)) {
//		/* Expects in order of data, then m_data_length telling the package items size */
//
//		write_to(&m_version, sizeof(m_version));
//		write_to(&m_type, sizeof(m_type));
//		write_to(&m_data_length, sizeof(m_data_length));
//
//		for (data_length_t i = 0; i < m_data_length; ++i) {
//			write_to(&m_array_of_data[i], sizeof(data));
//		}
//	}
//	
//	const auto get_version() const { return m_version; }
//	const auto get_type() const { return m_type; }
//	const auto get_items_length() const { return m_data_length; }
//	const data* get_item_index(const data_length_t idx) { 
//		if (idx < m_data_length) return &m_array_of_data[idx];
//		return nullptr;
//	}
//	const data* find_item(const char* key) { 
//		for (auto* i = m_array_of_data.get(); i != m_array_of_data.get() + m_data_length; ++i) { 
//			if (const auto kl = strlen(key), gl = strlen(i->get_key()); gl == kl && memcmp(i->get_key(), key, gl) == 0) 
//				return i;
//		}
//		return nullptr;
//	}
//
//	bool operator==(const package& o) {
//		return 
//			m_version == o.m_version && 
//			m_type == o.m_type && 
//			m_data_length == o.m_data_length && 
//			memcmp(m_array_of_data.get(), o.m_array_of_data.get(), m_data_length) == 0;
//	}
//private:
//	/* Versioning is important */
//	protocol_version_t m_version = static_cast<protocol_version_t>(protocol_version::V0_0_1);
//	/* Type of the device being built here in this object */
//	source_type_t m_type = 0;
//	/* How many items are stored here? (related to m_array_of_data) */
//	data_length_t m_data_length = 0;
//	/* Array of items to be read, zero to inf */
//	std::unique_ptr<data[]> m_array_of_data;
//};

// trying to simplify experience

#include <Arduino.h>

constexpr auto package_path_size = 1 << 6;

struct SerialPackage {
	enum class data_type : uint8_t {T_UNKNOWN, T_F, T_D, T_I, T_U};
	
	struct _ {
		char m_path[package_path_size]{};
		uint8_t m_type = static_cast<uint8_t>(data_type::T_UNKNOWN);

		union __ {
			float f;
			double d;
			int64_t i;
			uint64_t u;

			__() = default;
			__(float a) : f(a) {}
			__(double a) : d(a) {}
			__(int64_t a) : i(a) {}
			__(uint64_t a) : u(a) {}
		} m_data;

		void _cpystr(const char* s) {
			const auto len = strlen(s);
			if (len < package_path_size - 1) {
				memcpy(m_path, s, len);
				m_path[len] = '\0';
			}
			else {
				m_path[0] = '\0';
			}
		}

		_() = default;
		_(const char* path, float a) 	: m_type(static_cast<uint8_t>(data_type::T_F)), m_data(a) { _cpystr(path); }
		_(const char* path, double a) 	: m_type(static_cast<uint8_t>(data_type::T_D)), m_data(a) { _cpystr(path); }
		_(const char* path, int64_t a) 	: m_type(static_cast<uint8_t>(data_type::T_I)), m_data(a) { _cpystr(path); }
		_(const char* path, uint64_t a) : m_type(static_cast<uint8_t>(data_type::T_U)), m_data(a) { _cpystr(path); }
	} m_raw;

	SerialPackage() = default;

	template<typename T> SerialPackage(const char* path, T const& val) : m_raw(path, val) {}

	uint8_t* get_data() { return (uint8_t*)this; }
	size_t get_size() const { return sizeof(SerialPackage); }
};

inline void post(const char* path, const float val) {
	SerialPackage pkg(path, val);
	Serial.write(pkg.get_data(), pkg.get_size());
}

inline bool read(SerialPackage& pkg)
{
	const int to_read = Serial.available();
	if (to_read < sizeof(SerialPackage)) return false;
	Serial.readBytes(pkg.get_data(), pkg.get_size());
	return true;
}