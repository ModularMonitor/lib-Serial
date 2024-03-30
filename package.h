#include <Arduino.h>

namespace MMSerial {

	constexpr auto package_path_size = 1 << 6;
	constexpr auto serial_speed = 115200;

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

	inline void setupSerial()
	{
		Serial0.begin(serial_speed);
		while(!Serial0);
	}

	inline void post(const char* path, const float val) {
		SerialPackage pkg(path, val);
		Serial0.write(pkg.get_data(), pkg.get_size());
	}

	inline bool read(SerialPackage& pkg)
	{
		const int to_read = Serial0.available();
		if (to_read < 0) return false;
		if (static_cast<unsigned>(to_read) < sizeof(SerialPackage)) return false;
		Serial0.readBytes(pkg.get_data(), pkg.get_size());
		return true;
	}

}