#include <Arduino.h>
#include <deque>
#include <memory>

namespace MMSerial {

	enum class device_id : uint16_t {
		MASTER = 0,
		DHT22		/* Paths expected: `/dht22/temperature` [float], `/dht22/humidity` [float] */
	};

	constexpr auto package_path_size = 1 << 6;

	// using 10000 hz should be fast enough
	constexpr int lane_signal = 3; // send pulse for interrupt
	constexpr int lane_data = 4; // send data each pulse
	constexpr decltype(micros()) lane_data_interval = 500; // us

	enum class data_type : uint8_t { T_UNKNOWN, T_REQUESTONLY, T_F, T_D, T_I, T_U };

	struct data {

		uint16_t recvd_id = 0; // MUST BE FIRST

		uint8_t type = static_cast<uint8_t>(data_type::T_UNKNOWN);
		char path[package_path_size]{};
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
		} value{0LL};

		void _cpystr(const char* s) {
			const auto len = strlen(s);
			if (len < package_path_size - 1) { memcpy(path, s, len); path[len] = '\0'; }
			else { path[0] = '\0'; }
		}

		data() = default;
		data(const char* path_, float    val) : type(static_cast<uint8_t>(data_type::T_F)), value(val) { _cpystr(path_); }
		data(const char* path_, double   val) : type(static_cast<uint8_t>(data_type::T_D)), value(val) { _cpystr(path_); }
		data(const char* path_, int64_t  val) : type(static_cast<uint8_t>(data_type::T_I)), value(val) { _cpystr(path_); }
		data(const char* path_, uint64_t val) : type(static_cast<uint8_t>(data_type::T_U)), value(val) { _cpystr(path_); }

		void make_as_request(uint16_t id) {
			recvd_id = id;
			type = static_cast<uint8_t>(data_type::T_REQUESTONLY);
			memset(path, '\0', sizeof(path));
		}

		const char* get_path() const { return path; }
		data_type get_type() const { return static_cast<data_type>(type); }
		float get_as_float() const { return value.f; }
		double get_as_double() const { return value.d; }
		int64_t get_as_int() const { return value.i; }
		uint64_t get_as_uint() const { return value.u; }
	};

	struct ctl {
		decltype(micros()) last_read = 0;
		size_t bit_offset = 0;
		std::deque<std::unique_ptr<data>> seq;
		uint16_t own_id = 0; // MASK, 0 = master, reads everyone, default: only read their ID

		void push_bit(bool bit) {
			// be sure to not get broken packages. Skip if in the middle of one (like on startup)
			if (bit_offset == 0 && micros() - last_read < lane_data_interval) {
				last_read = micros();
				return;
			}

			last_read = micros();
			if (bit_offset == 0) seq.push_back(std::unique_ptr<data>(new data()));

			const auto* targ_real = seq.back().get();
			uint8_t* targ = (uint8_t*)targ_real;

			const size_t pi = bit_offset / 8;
			const size_t pbf = bit_offset % 8;

			targ[pi] = (targ[pi] & ~(1 << pbf)) | (bit ? (1 << pbf) : 0);

			if (bit_offset == (sizeof(targ_real->recvd_id) + sizeof(targ_real->type))) { // got number and command already, can check mask
				if (
					!( /* erase if not */
					/* receive only command targeted if not master and it's request */
					(targ_real->recvd_id == own_id && targ_real->type == static_cast<uint8_t>(data_type::T_REQUESTONLY)) 
					||
					/* or when master, allow recv data from others (not request) */
					(own_id == 0 && targ_real->type != static_cast<uint8_t>(data_type::T_REQUESTONLY))
					)
				)
				{
					seq.pop_back(); // free up
					//last_read = micros(); // ignore package
					bit_offset = 0; // wait next at 0
					return;
				}
			}

			if (++bit_offset >= sizeof(data) * 8) bit_offset = 0;
		}

		std::unique_ptr<data> pop()
		{
			if (seq.size() > 1 || (seq.size() > 0 && bit_offset == 0)) {
				std::unique_ptr<data> ptr = std::move(*seq.begin());
				seq.pop_front();
				return ptr;
			}
			return {};
		}
	};

	inline ctl _g_ctl;

	inline void _sendBit(bool bit)
	{
		digitalWrite(lane_data, bit); // based on random ppl, < 20 us
		delayMicroseconds(20);
		digitalWrite(lane_signal, 1); // based on random ppl, < 20 us
		delayMicroseconds(20);
		digitalWrite(lane_signal, 0); // based on random ppl, < 20 us
	}

	inline void _sendByte(uint8_t v)
	{

		for (size_t p = 0; p < 8; ++p)
			_sendBit((v >> p) & 1);
	}

	// triggered on lane_signal -> 1
	inline void _i_readBit()
	{
		_g_ctl.push_bit(digitalRead(lane_data));
	}

	inline void _sendData(uint8_t* data, size_t len)
	{
#ifndef _DEBUG
		detachInterrupt(digitalPinToInterrupt(lane_signal));
#endif

		while (len-- > 0) _sendByte(*data++);

		delayMicroseconds(500); // hold 500 for sync

#ifndef _DEBUG
		attachInterrupt(digitalPinToInterrupt(lane_signal), _i_readBit, RISING);
#endif
	}

	// User usable:

	template<typename T> 
	inline void send_package(const char* path, T val)
	{
		data dat(path, val);
		dat.recvd_id = _g_ctl.own_id;
		_sendData((uint8_t*)&dat, sizeof(dat));
	}

	inline void send_request(uint16_t rid)
	{
		data dat;
		dat.make_as_request(rid);
		_sendData((uint8_t*)&dat, sizeof(dat));
	}

	inline void setup(uint16_t own_id)
	{
		_g_ctl.own_id = own_id;
		attachInterrupt(digitalPinToInterrupt(lane_signal), _i_readBit, RISING);
	}

	inline void setup(device_id d_id)
	{
		setup(static_cast<uint16_t>(d_id));
	}

	inline std::unique_ptr<data> read_data()
	{
		return _g_ctl.pop();
	}
}