#ifndef VCD_IO_H
#define VCD_IO_H

/*************************************************************************************************************
 * VCDio - read and write Value Change Dump (.vcd) waveform files in C99.
 *
 * VCD is the text waveform format from IEEE 1364 (Verilog) / IEEE 1800 clause 21.7, understood by
 * GTKWave, Surfer, PulseView/sigrok and most simulators.
 *
 *   vcd_types.h  - status codes, timescales, signal types, time conversion
 *   vcd_writer.h - produce .vcd files, in time order or sorted for you
 *   vcd_reader.h - load .vcd files and query values over time
 *   vcd_uart.h   - draw UART frames into a writer and decode UART frames from a reader
 *
 * Including this header pulls in all of them.
 *************************************************************************************************************/

/* ================================================ INCLUDES =============================================== */
#include "vcd_reader.h"
#include "vcd_types.h"
#include "vcd_uart.h"
#include "vcd_writer.h"

#endif /* VCD_IO_H */
