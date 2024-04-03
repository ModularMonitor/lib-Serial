#include <Arduino.h>
#include <deque>
#include <memory>
#include <mutex>

namespace MMSerial {

	enum class device_id : uint16_t {
		MASTER = 0,
		DHT22		/* Paths expected: `/dht22/temperature` [float], `/dht22/humidity` [float] */
	};

	constexpr auto package_path_size = 1 << 6;

	// using 10000 hz should be fast enough
	constexpr int lane_signal = 2; // send pulse for interrupt
	constexpr int lane_data = 4; // send data each pulse
	constexpr decltype(micros()) lane_data_interval = 50000; // us

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
		} value{ 0LL };

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

		std::mutex sync_mng;
		char raw_data[sizeof(data)];
		size_t raw_data_off_bit = 0; // max 8 * sizeof(data)

		uint16_t own_id = 0; // MASK, 0 = master, reads everyone, default: only read their ID

		void push_bit(bool bb) {
			if (raw_data_off_bit >= sizeof(data) * 8) return; // lost package

			const size_t bit = raw_data_off_bit % 8;
			const size_t byte = raw_data_off_bit / 8;
			++raw_data_off_bit;

			raw_data[byte] = (raw_data[byte] & ~(1 << bit)) | (bb ? (1 << bit) : 0);
		}

		// for better results, call it at least once each 'lane_data_interval' us. Best if twice.
		void check_and_push_internal_mem()
		{
			if (raw_data_off_bit >= sizeof(data) * 8) { // has data
				std::lock_guard<std::mutex> l(sync_mng);

				data* targ_real = (data*)raw_data;
				if (
					/* receive only command targeted if not master and it's request */
					(targ_real->recvd_id == own_id && targ_real->type == static_cast<uint8_t>(data_type::T_REQUESTONLY))
					||
					/* or when master, allow recv data from others (not request) */
					(own_id == 0 && targ_real->type != static_cast<uint8_t>(data_type::T_REQUESTONLY))
					)
				{
					auto ndata = std::unique_ptr<data>(new data());
					memcpy(ndata.get(), targ_real, sizeof(data));
					seq.push_back(std::move(ndata));
					memset(raw_data, '\0', sizeof(raw_data));
					raw_data_off_bit = 0;
				}
			}
		}

		std::unique_ptr<data> pop()
		{
			if (seq.size() > 1 || (seq.size() > 0 && bit_offset == 0)) {
				std::lock_guard<std::mutex> l(sync_mng);
				std::unique_ptr<data> ptr = std::move(*seq.begin());
				seq.pop_front();
				return ptr;
			}
			return {};
		}
	};

	inline ctl& _g_ctl_get() {
		static ctl _g_ctl;
		return _g_ctl;
	}

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
		_g_ctl_get().push_bit(digitalRead(lane_data));
	}

	inline void _sendData(uint8_t* data, size_t len)
	{
		detachInterrupt(digitalPinToInterrupt(lane_signal));

		while (len-- > 0) _sendByte(*data++);

		attachInterrupt(digitalPinToInterrupt(lane_signal), _i_readBit, RISING);

		delayMicroseconds(lane_data_interval); // hold for sync
	}

	inline void _loop2_mem_check_task(void* p)
	{
		while (1) {
			_g_ctl_get().check_and_push_internal_mem();
			delayMicroseconds(lane_data_interval / 8);
		}
	}

	// User usable:

	template<typename T>
	inline void send_package(const char* path, T val)
	{
		pinMode(lane_signal, OUTPUT);
		pinMode(lane_data, OUTPUT);
		data dat(path, val);
		dat.recvd_id = _g_ctl_get().own_id;
		_sendData((uint8_t*)&dat, sizeof(dat));
		pinMode(lane_signal, INPUT);
		pinMode(lane_data, INPUT);
	}

	inline void send_request(uint16_t rid)
	{
		pinMode(lane_signal, OUTPUT);
		pinMode(lane_data, OUTPUT);
		data dat;
		dat.make_as_request(rid);
		_sendData((uint8_t*)&dat, sizeof(dat));
		pinMode(lane_signal, INPUT);
		pinMode(lane_data, INPUT);
	}

	inline void setup(uint16_t own_id)
	{
		static TaskHandle_t thr_worker;
		_g_ctl_get().own_id = own_id;
		pinMode(lane_signal, INPUT);
		pinMode(lane_data, INPUT);
		attachInterrupt(digitalPinToInterrupt(lane_signal), _i_readBit, RISING);
		if (!thr_worker) {
			xTaskCreate(_loop2_mem_check_task, "MMSASYNC", 2048, nullptr, 0, &thr_worker);
		}
	}

	inline void setup(device_id d_id)
	{
		setup(static_cast<uint16_t>(d_id));
	}

	inline std::unique_ptr<data> read_data()
	{
		return _g_ctl_get().pop();
	}
}