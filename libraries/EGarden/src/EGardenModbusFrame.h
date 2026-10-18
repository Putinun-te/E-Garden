// Modbus RTU helpers for the RS485 pH/EC probes. Plain C++ (unit tested on a PC).
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace eg {
namespace modbus {

uint16_t crc16(const uint8_t* data, size_t len);  // Modbus CRC (poly 0xA001, init 0xFFFF)

// Builds a "read holding registers" (function 0x03) request. out must hold 8 bytes.
void buildReadRequest(uint8_t slave, uint16_t firstRegister, uint16_t count, uint8_t out[8]);

// Expected response length for a read of `count` registers: addr, fn, bytecount, data, crc.
constexpr size_t responseLength(uint16_t count) { return 5 + 2 * (size_t)count; }

enum class FrameError : uint8_t { None, Short, BadCrc, WrongSlave, Exception, WrongFunction, WrongLength };
const char* frameErrorName(FrameError e);

// Validates a response to buildReadRequest(slave, ..., count) and copies the
// register values into regs (count entries).
FrameError parseReadResponse(const uint8_t* frame, size_t len, uint8_t slave, uint16_t count, uint16_t* regs);

// The v1 sketches used these fixed requests (bytes copied from LoRa_Water3.2).
constexpr uint8_t PH_TEMP_SLAVE = 0x05;  // registers 0..1: temperature x10, pH x10
constexpr uint16_t PH_TEMP_FIRST = 0x0000, PH_TEMP_COUNT = 2;
constexpr uint8_t EC_SLAVE = 0x0A;       // register 1: EC x10
constexpr uint16_t EC_FIRST = 0x0001, EC_COUNT = 1;

}  // namespace modbus
}  // namespace eg
