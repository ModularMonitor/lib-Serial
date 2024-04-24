#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <deque>
#include <memory>
#include <mutex>

/*
How to use it:

In the setup function, call:

```
CustomSerial::begin_slave(my_id, on_request_do);
```

and define the function to answer with something, like:

```
void on_request_do()
{  
  CustomSerial::command_package cmd(my_id, 
    "/data", 50ULL,
    "/random_data", 12.456f
  );
  
  CustomSerial::write(cmd);
}
```
*/

namespace CustomSerial {

	constexpr int default_led_pin = 2;
	constexpr int default_port_sda = 5;
	constexpr int default_port_scl = 4;
	constexpr int port_speed_baud = 40000;
	constexpr size_t max_packages_at_once = 16;
	
	enum class device_id : uint8_t {
		DHT22_SENSOR,	   /* Temperature and Humidity sensor */
		MICS_6814_SENSOR,  /* CO, NH3 and NO2 sensor */
		KY038_HW038_SENSOR,/* Loudness and lightness sensor */
		GY87_SENSOR,	   /* Accelerometer, temperature, pressure, altitude and compass sensor */
		CCS811_SENSOR,	   /* Quality of air sensor */
		PMSDS011_SENSOR,   /* Nova PM sensor */		
		BATTERY_SENSOR,    /* Own battery reporting sensor */
		_MAX			   /* MAX to limit testing of devices */
	};
	
	constexpr uint8_t device_to_uint8t(device_id id) { return static_cast<uint8_t>(id); }

	struct devices_online {
		bool m_by_id[static_cast<size_t>(device_id::_MAX)]{false};
		bool m_was_checked = false;

		bool has_checked_once() const { return m_was_checked; }
		void set_checked() { m_was_checked = true; }

		void set_online(device_id id, bool answered) {
			m_by_id[device_to_uint8t(id)] = answered;
		}
		void set_online(uint8_t id, bool answered) {
			m_by_id[id] = answered;
		}
		bool is_online(device_id id) const {
			return m_by_id[device_to_uint8t(id)];
		}
		bool is_online(uint8_t id) const {
			return m_by_id[id];
		}

		bool has_any_online() const {
			for (uint8_t p = 0; p < device_to_uint8t(device_id::_MAX); ++p) {
				if (m_by_id[p] == true) return true;
			}
			return false;
		}
	};	

	struct command {
		enum class vtype : uint8_t { INVALID, TD, TF, TI, TU, REQUEST = std::numeric_limits<uint8_t>::max() };

		union _ {
			double d;
			float f;
			int64_t i;
			uint64_t u;

			_() : d(0.0) {}
			_(double _d) : d(_d) {}
			_(float _f) : f(_f) {}
			_(int64_t _i) : i(_i) {}
			_(uint64_t _u) : u(_u) {}
			void operator=(double _d) { d = _d; }
			void operator=(float _f) { f = _f; }
			void operator=(int64_t _i) { i = _i; }
			void operator=(uint64_t _u) { u = _u; }
		};

	private:
		char path[16]{};
		_ m_val{0.0f};
		uint8_t m_id = 0;
		uint8_t m_type = static_cast<uint8_t>(vtype::INVALID);

		void _set_val(double _d)   { m_val = _d; m_type = static_cast<uint8_t>(vtype::TD); }
		void _set_val(float _f)    { m_val = _f; m_type = static_cast<uint8_t>(vtype::TF); }
		void _set_val(int64_t _i)  { m_val = _i; m_type = static_cast<uint8_t>(vtype::TI); }
		void _set_val(uint64_t _u) { m_val = _u; m_type = static_cast<uint8_t>(vtype::TU); }
	public:		
		command() = default;

		template<typename T>
		void make_data(uint8_t of, const char* path15, const T& value)
		{
			m_id = of;
			const auto len = strlen(path15);
			memcpy(path, path15, (len + 1) > 16 ? 16 : len + 1);
			_set_val(value);
		}

		template<typename T>
		void make_data(device_id of, const char* path15, const T& value)
		{
			make_data(device_to_uint8t(of), path15, value);
		}

		device_id get_device_id() const { return static_cast<device_id>(m_id); }
		const char* get_path() const { return path; }
		const _& get_val() const { return m_val; }
		vtype get_val_type() const { return static_cast<vtype>(m_type); }

		bool valid() const { return  m_type != static_cast<uint8_t>(vtype::INVALID); }
	};

	struct command_package {
		command cmd[max_packages_at_once];
		//uint16_t num_cmds = 0;
		
		command_package() = default;
		command_package(const command_package& c) { memcpy((void*)this, (void*)&c, sizeof(command_package)); }
		command_package(command_package&& c) { memcpy((void*)this, (void*)&c, sizeof(command_package)); }
		void operator=(const command_package& c) { memcpy((void*)this, (void*)&c, sizeof(command_package)); }
		void operator=(command_package&& c) { memcpy((void*)this, (void*)&c, sizeof(command_package)); }

		template<typename T, typename... Args>
		void _build(device_id sid, const char* path, const T& val, Args... others)
		{
			constexpr size_t off = sizeof...(others) / 2;
			static_assert (off < max_packages_at_once, "Can't hold that many info in one package!");

			cmd[off].make_data(sid, path, val);
			_build(sid, others...);
		}
		template<typename T>
		void _build(device_id sid, const char* path, const T& val)
		{
			cmd[0].make_data(sid, path, val);
		}

		template<typename T>
		command_package(device_id sid, const char* path, const T& val)
		{
			//num_cmds = 1;
			_build(sid, path, val);
		}
		template<typename T, typename... Args>
		command_package(device_id sid, const char* path, const T& val, Args... others)
		{
			//num_cmds = 1 + (sizeof...(others) / 2);
			_build(sid, path, val, others...);
		}

		command& idx(const size_t p) {
			if (p < max_packages_at_once) return cmd[p];
			throw std::out_of_range("Index must not be bigger than the limit!");
		}

		const command& idx(const size_t p) const {
			if (p < max_packages_at_once) return cmd[p];
			throw std::out_of_range("Index must not be bigger than the limit!");
		}

		size_t size() const { 
			for(size_t p = 0; p < max_packages_at_once; p++) {
				if (!cmd[p].valid()) return p;
			}
			return max_packages_at_once;
		}

	};

	struct config {
		void (*request_cb)(void) = nullptr;
		int led = default_led_pin;
		bool was_led_on = false;
	};

	config& _get_config()
	{
		static config cfg;
		return cfg;
	}

	void _toggle_led()
	{
		auto& cfg = _get_config();
		if (cfg.led >= 0) digitalWrite(cfg.led, cfg.was_led_on = !cfg.was_led_on);
	}

	void _reset_led()
	{
		auto& cfg = _get_config();
		if (cfg.led >= 0) digitalWrite(cfg.led, cfg.was_led_on = false);
	}

	void _handle_event()
	{
		const auto& cfg = _get_config();

		if (cfg.request_cb == nullptr) return;
		cfg.request_cb();
		_toggle_led();
	}

	void begin_master(const int port_sda = default_port_sda, const int port_scl = default_port_scl)
	{
		auto& cfg = _get_config();

		cfg.led = -1;

		Wire1.begin(port_sda, port_scl, port_speed_baud);
	}

	void begin_slave(uint8_t sid, void(*request_callback)(void), const int port_sda = default_port_sda, const int port_scl = default_port_scl, const int led_pin = default_led_pin)
	{
		auto& cfg = _get_config();

		cfg.led = led_pin;
		cfg.request_cb = request_callback;

		pinMode(cfg.led, OUTPUT);

		Wire1.begin(sid, port_sda, port_scl, port_speed_baud);
		Wire1.onRequest(_handle_event);
	}

	void begin_slave(device_id sid, void(*request_callback)(void), const int port_sda = default_port_sda, const int port_scl = default_port_scl, const int led_pin = default_led_pin)
	{
		auto& cfg = _get_config();

		cfg.led = led_pin;
		cfg.request_cb = request_callback;

		pinMode(cfg.led, OUTPUT);

		Wire1.begin(device_to_uint8t(sid), port_sda, port_scl, port_speed_baud);
		Wire1.onRequest(_handle_event);
	}

	void end()
	{
		_reset_led();
		Wire1.end();
	}

	void request(uint8_t sid)
	{
		Wire1.requestFrom(sid, sizeof(command_package), false);
	}

	void request(device_id sid)
	{
		Wire1.requestFrom(static_cast<uint8_t>(sid), sizeof(command_package), false);
	}

	bool read(command_package& o)
	{
		if (Wire1.available() < sizeof(command_package)) return false;
		Wire1.readBytes((uint8_t*)&o, sizeof(command_package));
		return true;
	}

	void write(const command_package& o)
	{
		Wire1.write((uint8_t*)&o, sizeof(command_package));
	}

	devices_online& get_devices_list() 
	{
		static devices_online dev;
		return dev;
	}

	void check_all_devices_online()
	{
		auto& dl = get_devices_list();
		constexpr uint8_t max_item = device_to_uint8t(device_id::_MAX);

		for(uint8_t p = 0; p < max_item; ++p) {
			command_package cmd;
			request(p);
			dl.set_online(p, read(cmd) && cmd.size() > 0);
		}

		dl.set_checked();
	}

	void check_devices_online_if_not_checked()
	{
		const auto& dl = get_devices_list();
		if (!dl.has_checked_once()) check_all_devices_online();
	}

	uint8_t get_devices_limit()
	{
		return device_to_uint8t(device_id::_MAX);
	}

	void set_pin_to_check_devices(const int port_id)
	{
		attachInterrupt(digitalPinToInterrupt(port_id), check_all_devices_online, RISING);
	}
	
	void unset_pin_to_check_devices(const int port_id)
	{		
		detachInterrupt(digitalPinToInterrupt(port_id));
	}

	bool is_device_connected(device_id id)
	{
		const auto& dl = get_devices_list();
		return dl.is_online(id);
	}

	bool is_device_connected(uint8_t id)
	{
		const auto& dl = get_devices_list();
		return dl.is_online(id);
	}

	bool is_any_device_connected()
	{
		const auto& dl = get_devices_list();
		return dl.has_any_online();
	}

}